#include <gtest/gtest.h>

#include "tokenizer.h"

TEST(PorterStemmerTest, EmptyString) {
    PorterStemmer stemmer;
    std::string w = "";
    stemmer.stem(w);
    EXPECT_EQ(w, "");
}

TEST(PorterStemmerTest, ShortWordsUnchanged) {
    PorterStemmer stemmer;
    std::string w1 = "a";
    std::string w2 = "be";
    stemmer.stem(w1);
    stemmer.stem(w2);
    EXPECT_EQ(w1, "a");
    EXPECT_EQ(w2, "be");
}

TEST(PorterStemmerTest, SsAndSuffices) {
    PorterStemmer stemmer;
    std::string w1 = "caresses";
    std::string w2 = "ponies";
    std::string w3 = "ties";
    std::string w4 = "cats";

    stemmer.stem(w1);
    stemmer.stem(w2);
    stemmer.stem(w3);
    stemmer.stem(w4);

    EXPECT_EQ(w1, "caress");
    EXPECT_EQ(w2, "poni");
    EXPECT_EQ(w3, "ti");
    EXPECT_EQ(w4, "cat");
}

TEST(PorterStemmerTest, EedSuffix) {
    PorterStemmer stemmer;
    std::string w = "agreed";
    stemmer.stem(w);
    EXPECT_EQ(w, "agre");
}

TEST(PorterStemmerTest, EdIngAndDoubleConsonant) {
    PorterStemmer stemmer;
    std::string w1 = "plastered";
    std::string w2 = "hopping";
    std::string w3 = "hoped";

    stemmer.stem(w1);
    stemmer.stem(w2);
    stemmer.stem(w3);

    EXPECT_EQ(w1, "plast");
    EXPECT_EQ(w2, "hop");
    EXPECT_EQ(w3, "hope");
}

TEST(PorterStemmerTest, YToI) {
    PorterStemmer stemmer;
    std::string w = "happy";
    stemmer.stem(w);
    EXPECT_EQ(w, "happi");
}

TEST(PorterStemmerTest, Step2And3) {
    PorterStemmer stemmer;
    std::string w1 = "relational";
    std::string w2 = "electricity";
    std::string w3 = "formalize";

    stemmer.stem(w1);
    stemmer.stem(w2);
    stemmer.stem(w3);

    EXPECT_EQ(w1, "rel");
    EXPECT_EQ(w2, "electr");
    EXPECT_EQ(w3, "form");
}

TEST(PorterStemmerTest, Idempotence) {
    PorterStemmer stemmer;
    std::string w = "running";
    stemmer.stem(w);
    std::string first = w;
    stemmer.stem(w);
    EXPECT_EQ(w, first);
}

TEST(PorterStemmerTest, ConsistencyBetweenInstances) {
    PorterStemmer s1;
    PorterStemmer s2;
    std::string a = "running";
    std::string b = "running";
    s1.stem(a);
    s2.stem(b);
    EXPECT_EQ(a, b);
}

TEST(PorterStemmerTest, Step2Mappings) {
    PorterStemmer stemmer;
    std::vector<std::pair<std::string, std::string>> cases = {
        {"activational", "activ"},     {"conditional", "condit"},    {"valenci", "val"},
        {"hesitanci", "hesit"},        {"standardizer", "standard"}, {"allowabli", "allow"},
        {"rationalization", "ration"}, {"electrical", "electr"}};

    for (auto &p : cases) {
        std::string w = p.first;
        stemmer.stem(w);
        EXPECT_EQ(w, p.second) << "Input: " << p.first;
    }
}

TEST(PorterStemmerTest, Step3Mappings) {
    PorterStemmer stemmer;
    std::vector<std::pair<std::string, std::string>> cases = {{"triplicate", "tripl"},
                                                              {"formative", "form"},
                                                              {"formalize", "form"},
                                                              {"electricicity", "electric"},
                                                              {"hopefulness", "hope"}};

    for (auto &p : cases) {
        std::string w = p.first;
        stemmer.stem(w);
        EXPECT_EQ(w, p.second) << "Input: " << p.first;
    }
}

TEST(PorterStemmerTest, Step4RemovalsAndIon) {
    PorterStemmer stemmer;
    std::vector<std::pair<std::string, std::string>> cases = {
        {"adjustment", "adjust"}, {"adjustable", "adjust"}, {"conclusion", "conclus"}, {"action", "act"}, {"region", "region"}};

    for (auto &p : cases) {
        std::string w = p.first;
        stemmer.stem(w);
        EXPECT_EQ(w, p.second) << "Input: " << p.first;
    }
}
