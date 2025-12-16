#include "tokenizer.h"

#include <cctype>

Tokenizer::Tokenizer(std::unique_ptr<IStemmer> stemmer) : stemmer(std::move(stemmer)) {}

Tokenizer::Tokenizer() : stemmer(std::make_unique<DummyStemmer>()) {}

std::vector<std::string> Tokenizer::getRawTokens(const std::string_view& query) {
    std::vector<std::string> rawTokens;
    std::string currentToken = "";
    for (size_t i = 0; i < query.length(); ++i) {
        char c = query[i];
        bool is_op_char = (c == '(' || c == ')' || c == '!' || c == '&' || c == '|');
        if (c == ' ' || is_op_char) {
            if (!currentToken.empty()) {
                rawTokens.push_back(currentToken);
                currentToken.clear();
            }
            if (is_op_char) {
                rawTokens.push_back(std::string(1, c));
            }
        } else {
            currentToken += c;
        }
    }
    if (!currentToken.empty()) rawTokens.push_back(currentToken);

    return rawTokens;
}

void Tokenizer::tokenize(const std::string_view& text) {
    tokens.clear();
    tokens.reserve(text.size() / 6);

    total_len = 0;

    std::string current_token;
    current_token.reserve(32);

    int dots_in_token = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char raw_c = static_cast<unsigned char>(text[i]);
        char c = std::tolower(raw_c);

        bool is_basic_char = std::isalnum(raw_c) || c == '_';
        bool should_include = false;

        if (c == '.' || c == ',') {
            if (dots_in_token == 0 && !current_token.empty() && std::isdigit(current_token.back())) {
                if (i + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[i + 1]))) {
                    should_include = true;
                    dots_in_token++;
                }
            }
        }

        if (c == '-') {
            if (current_token.empty()) {
                if (i + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[i + 1]))) {
                    should_include = true;
                }
            } else {
                if (std::isalpha(current_token.back()) && i + 1 < text.size() &&
                    std::isalpha(static_cast<unsigned char>(text[i + 1]))) {
                    should_include = true;
                }
            }
        }

        if (c == '\'') {
            if (!current_token.empty() && i + 1 < text.size() && std::isalnum(static_cast<unsigned char>(text[i + 1]))) {
                should_include = true;
            }
        }

        if (is_basic_char || should_include) {
            current_token += c;
        } else {
            if (!current_token.empty()) {
                std::string stemmed_token = stemmer->stem(current_token);
                tokens.push_back(stemmed_token);
                total_len += current_token.size();
                current_token.clear();
                dots_in_token = 0;
            }
        }
    }

    if (!current_token.empty()) {
        std::string stemmed_token = stemmer->stem(current_token);
        tokens.push_back(stemmed_token);
        total_len += current_token.size();
    }
}

std::vector<std::string> Tokenizer::getTokens() const { return tokens; }

size_t Tokenizer::tokensAmount() const { return tokens.size(); }

double Tokenizer::avgTokenLen() const { return tokens.size() == 0 ? 0 : double(total_len) / tokens.size(); }

std::string DummyStemmer::stem(std::string& word) { return word; }

bool PorterStemmer::isVowel(int i) const {
    if (i < 0 || i > end) return false;
    char c = word[i];
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') return true;
    if (c == 'y') {
        return (i == 0) || !isVowel(i - 1);
    }
    return false;
}

int PorterStemmer::getMeasure(int limit) const {
    int measure = 0;
    bool vc = false;

    for (int i = 0; i <= limit; ++i) {
        char c = word[i];
        if (isVowel(i)) {
            vc = false;
        } else {
            if (!vc) {
                ++measure;
                vc = true;
            }
        }
    }
    return measure;
}

bool PorterStemmer::m_condition(int minM) const { return getMeasure(j) > minM; }

bool PorterStemmer::hasVowel() const {
    for (int i = 0; i <= j; ++i) {
        if (isVowel(i)) return true;
    }
    return false;
}

bool PorterStemmer::isDoubleConsonant(int i) const {
    if (i < 1 || isVowel(i) || isVowel(i - 1)) return false;
    return (word[i] == word[i - 1]);
}

bool PorterStemmer::isCVC(int i) const {
    if (i < 2 || isVowel(i) || !isVowel(i - 1) || isVowel(i - 2)) return false;
    char c = word[i];
    return !(c == 'w' || c == 'x' || c == 'y');
}

bool PorterStemmer::endsWith(const std::string& w, const std::string& suffix) const {
    if (w.size() < suffix.size()) return false;
    return w.substr(w.size() - suffix.size()) == suffix;
}

std::string PorterStemmer::replaceSuffixIfMeasure(std::string& w, const std::string& suffix, const std::string& replacement,
                                                  int minM) {
    if (!endsWith(w, suffix)) return w;

    size_t suffix_len = suffix.size();
    size_t current_size = w.size();

    j = current_size - suffix_len - 1;

    if (m_condition(minM)) {
        w.resize(current_size - suffix_len);
        w += replacement;
        end = w.size() - 1;
    }
    return w;
}

std::string PorterStemmer::replaceSuffixIfVowel(std::string& w, const std::string& suffix, const std::string& replacement) {
    if (!endsWith(w, suffix)) return w;

    size_t suffix_len = suffix.size();
    size_t current_size = w.size();

    j = current_size - suffix_len - 1;

    if (hasVowel()) {
        w.resize(current_size - suffix_len);
        w += replacement;
        end = w.size() - 1;
    }
    return w;
}

void PorterStemmer::step1() {
    if (end < 0) return;

    if (endsWith(word, "sses")) {
        word.resize(word.size() - 2);
        end -= 2;
    } else if (endsWith(word, "ies")) {
        word.resize(word.size() - 3);
        word += 'i';
        end -= 2;
    } else if (endsWith(word, "ss")) {
    } else if (endsWith(word, "s")) {
        if (end > 0 && word.at(end - 1) != 's') {
            word.pop_back();
            end--;
        }
    }

    bool rule1b_applied = false;

    if (endsWith(word, "eed")) {
        word = replaceSuffixIfMeasure(word, "eed", "ee", 0);
    } else {
        if (endsWith(word, "ed")) {
            word = replaceSuffixIfVowel(word, "ed", "");
            if (word.size() < end + 2) rule1b_applied = true;
        } else if (endsWith(word, "ing")) {
            word = replaceSuffixIfVowel(word, "ing", "");
            if (word.size() < end + 3) rule1b_applied = true;
        }
    }

    if (rule1b_applied) {
        char last = word.back();
        if (endsWith(word, "at") || endsWith(word, "bl") || endsWith(word, "iz")) {
            word += 'e';
            end++;
        } else if (isDoubleConsonant(end) && !(last == 'l' || last == 's' || last == 'z')) {
            word.pop_back();
            end--;
        } else if (getMeasure(end) == 1 && isCVC(end)) {
            word += 'e';
            end++;
        }
    }

    if (end > 0 && word.back() == 'y' && !isVowel(end - 1)) {
        word.back() = 'i';
    }
}

void PorterStemmer::step2() {
    if (word.size() <= 2) return;

    if (replaceSuffixIfMeasure(word, "ational", "ate", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "tional", "tion", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "enci", "ence", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "anci", "ance", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "izer", "ize", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "abli", "able", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "alli", "al", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "entli", "ent", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "eli", "e", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ousli", "ous", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ization", "ize", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ation", "ate", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ator", "ate", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "alism", "al", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "iveness", "ive", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "fulness", "ful", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ousness", "ous", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "aliti", "al", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "iviti", "ive", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "biliti", "ble", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "logi", "log", 0).size() < word.size()) return;
}

void PorterStemmer::step3() {
    if (word.size() <= 2) return;

    if (replaceSuffixIfMeasure(word, "icate", "ic", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ative", "", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "alize", "al", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "iciti", "ic", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ical", "ic", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ful", "", 0).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ness", "", 0).size() < word.size()) return;
}

void PorterStemmer::step4() {
    if (word.size() <= 2) return;

    if (replaceSuffixIfMeasure(word, "al", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ance", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ence", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "er", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ic", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "able", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ible", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ant", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ement", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ment", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ent", "", 1).size() < word.size()) return;

    if (endsWith(word, "ion")) {
        j = (int)word.size() - 4;
        if (m_condition(1) && (word[j] == 's' || word[j] == 't')) {
            word.resize(word.size() - 3);
            end -= 3;
            return;
        }
    }

    if (replaceSuffixIfMeasure(word, "ou", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ism", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ate", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "iti", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ous", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ive", "", 1).size() < word.size()) return;
    if (replaceSuffixIfMeasure(word, "ize", "", 1).size() < word.size()) return;
}

void PorterStemmer::step5() {
    if (word.back() == 'e') {
        j = end - 1;
        int m = getMeasure(j);

        if (m > 1 || (m == 1 && !isCVC(end - 1))) {
            word.pop_back();
            end--;
        }
    }

    if (word.back() == 'l' && word.size() > 1 && word[word.size() - 2] == 'l') {
        j = end - 1;
        if (getMeasure(j) > 1) {
            word.pop_back();
            end--;
        }
    }
}

std::string PorterStemmer::stem(std::string& input_word) {
    word = std::move(input_word);

    std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return std::tolower(c); });

    end = word.length() - 1;

    if (word.size() <= 2) return word;

    step1();  // множественное число и формы глаголов
    step2();  // длинные суффиксы, которые существительное<->прилагательное
    step3();  // абстрактные существительные -ative, -alize
    step4();  // оставшиеся общие суффиксы
    step5();  // финальная числа, -е -ll

    return word;
}