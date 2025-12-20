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
        Keys: bson.D{{Key: "last_scraped", Value: 1}},
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

	filter := bson.M{"normalized_url": doc.NormalizedURL}

	var existing struct {
		ContentHash string `bson:"content_hash"`
	}
	err := d.documents.FindOne(ctx, filter, options.FindOne().SetProjection(bson.M{"content_hash": 1})).Decode(&existing)

	var update bson.M
	opts := options.Update().SetUpsert(true)

	if err == mongo.ErrNoDocuments {
		doc.FirstScraped = time.Now().Unix()
		update = bson.M{
			"$set": bson.M{
				"normalized_url": doc.NormalizedURL,
				"title":          doc.Title,
				"html_content":   doc.HTMLContent,
				"content_hash":   doc.ContentHash,
				"content_length": len(doc.HTMLContent),
				"first_scraped":  doc.FirstScraped,
				"last_scraped":   doc.LastScraped,
				"last_modified":  doc.LastScraped,
			},
			"$inc": bson.M{"scraped_count": 1},
		}

	} else if err == nil {
		if existing.ContentHash == doc.ContentHash {
			update = bson.M{
				"$set": bson.M{
					"last_scraped":  doc.LastScraped,
				},
				"$inc": bson.M{"scraped_count": 1},
			}
		} else {
			update = bson.M{
				"$set": bson.M{
					"title":          doc.Title,
					"html_content":   doc.HTMLContent,
					"content_hash":   doc.ContentHash,
					"content_length": len(doc.HTMLContent),
					"last_scraped":   doc.LastScraped,
					"last_modified":  time.Now().Unix(),
				},
				"$inc": bson.M{"scraped_count": 1},
			}
		}

	} else {
		return fmt.Errorf("ошибка поиска документа: %w", err)
	}

	_, err = d.documents.UpdateOne(ctx, filter, update, opts)
	return err
}