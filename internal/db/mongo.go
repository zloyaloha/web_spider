package db

import (
	"context"
	"fmt"
	"log"
	"time"
	"web_spider/internal/config"
	"web_spider/internal/models"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
)

type MongoDB struct {
	client   *mongo.Client
	database *mongo.Database
	documents *mongo.Collection
	spiderState *mongo.Collection
	spiderHistory *mongo.Collection
}

func NewMongoDB(config config.DBConfig) (*MongoDB, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10 * time.Second)
	defer cancel()

	client, err := mongo.Connect(ctx, options.Client().ApplyURI(config.Connection))

	if err != nil {
		return nil, fmt.Errorf("failed to connect to MongoDB: %v", err)
	}

	if err := client.Ping(ctx, nil); err != nil {
		return nil, fmt.Errorf("can't ping MongoDB: %v", err)
	}

	db := client.Database(config.Database)

	d := &MongoDB{
		client:   client,
		database: db,
		documents: db.Collection(config.Collections.Documents),
	}

	if err := d.createIndexes(); err != nil {
		return nil, fmt.Errorf("can't create indicies: %v", err)
	}

	return d, nil
}


func (d *MongoDB) createIndexes() error {
    ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()

    indexModel := mongo.IndexModel{
        Keys: bson.D{{Key: "normalized_url", Value: 1}},
        Options: options.Index().SetUnique(true),
    }
    _, err := d.documents.Indexes().CreateOne(ctx, indexModel)
    if err != nil && err.Error() != "index already exists" {
        log.Printf("Ошибка при создании индекса URL: %v", err)
    }

    indexModel = mongo.IndexModel{
        Keys: bson.D{{Key: "last_scraped", Value: 1}}, // 1 = Ascending
    }
    _, err = d.documents.Indexes().CreateOne(ctx, indexModel)
    if err != nil && err.Error() != "index already exists" {
        log.Printf("Ошибка при создании индекса last_scraped: %v", err)
    }

    return nil
}

func (d *MongoDB) GetStaleDocuments(thresholdHours int, limit int) ([]string, error) {
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    cutoff := time.Now().Add(-time.Duration(thresholdHours) * time.Hour).Unix()

    filter := bson.M{
        "last_scraped": bson.M{"$lt": cutoff},
    }

    opts := options.Find().
        SetProjection(bson.M{"normalized_url": 1}).
        SetLimit(int64(limit))

    cursor, err := d.documents.Find(ctx, filter, opts)
    if err != nil {
        return nil, fmt.Errorf("ошибка поиска устаревших документов: %v", err)
    }
    defer cursor.Close(ctx)

    var urls []string
    type UrlOnly struct {
        NormalizedURL string `bson:"normalized_url"`
    }

    for cursor.Next(ctx) {
        var res UrlOnly
        if err := cursor.Decode(&res); err == nil {
            urls = append(urls, res.NormalizedURL)
        }
    }

    if err := cursor.Err(); err != nil {
        return nil, err
    }

    return urls, nil
}

func (d *MongoDB) GetDocument(normalizedURL string) (*models.Document, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	var doc models.Document
	err := d.documents.FindOne(ctx, bson.M{"normalized_url": normalizedURL}).Decode(&doc)
	if err == mongo.ErrNoDocuments {
		return nil, nil
	}
	return &doc, err
}


func (d *MongoDB) DocumentExists(normalizedURL string) (bool, error) {
	doc, err := d.GetDocument(normalizedURL)
	return doc != nil, err
}


func (d *MongoDB) Close() error {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	return d.client.Disconnect(ctx)
}

func (d *MongoDB) SaveDocument(doc *models.Document) error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	opts := options.Update().SetUpsert(true)
	filter := bson.M{"normalized_url": doc.NormalizedURL}

	var updateDoc bson.M
	data, _ := bson.Marshal(doc)
	bson.Unmarshal(data, &updateDoc)

	delete(updateDoc, "scraped_count")
	delete(updateDoc, "_id")

	update := bson.M{
		"$set": updateDoc,
		"$inc": bson.M{"scraped_count": 1},
	}

	_, err := d.documents.UpdateOne(ctx, filter, update, opts)
	return err
}

func (d *MongoDB) GetSourceStats(source string) (map[string]interface{}, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	pipeline := mongo.Pipeline{
		bson.D{{Key: "$match", Value: bson.D{{Key: "source", Value: source}}}},
		bson.D{{Key: "$group", Value: bson.D{
			{Key: "_id", Value: nil},
			{Key: "total_documents", Value: bson.D{{Key: "$sum", Value: 1}}},
			{Key: "avg_content_length", Value: bson.D{{Key: "$avg", Value: "$content_length"}}},
			{Key: "max_scraped_count", Value: bson.D{{Key: "$max", Value: "$scraped_count"}}},
			{Key: "valid_documents", Value: bson.D{{Key: "$sum", Value: bson.D{{Key: "$cond", Value: bson.A{"$is_valid", 1, 0}}}}}},
		}}},
	}

	cursor, err := d.documents.Aggregate(ctx, pipeline)
	if err != nil {
		return nil, err
	}
	defer cursor.Close(ctx)

	var results []map[string]interface{}
	if err := cursor.All(ctx, &results); err != nil {
		return nil, err
	}

	if len(results) == 0 {
		return make(map[string]interface{}), nil
	}

	return results[0], nil
}

func (d *MongoDB) SaveSpiderState(state *models.SpiderState) error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	opts := options.Update().SetUpsert(true)
	filter := bson.M{"source": state.Source}
	update := bson.M{"$set": state}

	_, err := d.spiderState.UpdateOne(ctx, filter, update, opts)
	return err
}

func (d *MongoDB) GetSpiderState(source string) (*models.SpiderState, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	var state models.SpiderState
	err := d.spiderState.FindOne(ctx, bson.M{"source": source}).Decode(&state)
	if err == mongo.ErrNoDocuments {
		return nil, nil
	}
	return &state, err
}

func (d *MongoDB) SaveSpiderHistory(history *models.SpiderHistory) error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	_, err := d.spiderHistory.InsertOne(ctx, history)
	return err
}
