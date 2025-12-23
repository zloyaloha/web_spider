#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>

#include "searcher.h"

static TermInfo T(int id, uint32_t tf = 1) {
    TermInfo t;
    t.doc_id = id;
    t.tf = tf;
    return t;
}

static std::vector<int> ids(const std::vector<TermInfo>& v) {
    std::vector<int> res;
    res.reserve(v.size());
    for (const auto& t : v) res.push_back(t.doc_id);
    return res;
}

static std::vector<TermInfo> make_range(int start, int count) {
    std::vector<TermInfo> v;
    v.reserve(count);
    for (int i = 0; i < count; ++i) v.push_back(T(start + i));
    return v;
}

class IntersectListsTest : public ::testing::Test {
protected:
    void CheckSorted(const std::vector<TermInfo>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_LT(v[i - 1].doc_id, v[i].doc_id) << "Vector not sorted at position " << i;
        }
    }
};

TEST_F(IntersectListsTest, BasicIntersection) {
    std::vector<TermInfo> l1 = {T(1), T(3), T(5), T(7)};
    std::vector<TermInfo> l2 = {T(3), T(5), T(9)};

    auto result = intersect_lists(l1, l2);

    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].doc_id, 3);
    EXPECT_EQ(result[1].doc_id, 5);
    CheckSorted(result);
}

TEST_F(IntersectListsTest, NoIntersection) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3)};
    std::vector<TermInfo> l2 = {T(4), T(5), T(6)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, EmptyFirstList) {
    std::vector<TermInfo> l1 = {};
    std::vector<TermInfo> l2 = {T(1), T(2), T(3)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, EmptySecondList) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3)};
    std::vector<TermInfo> l2 = {};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, BothListsEmpty) {
    std::vector<TermInfo> l1 = {};
    std::vector<TermInfo> l2 = {};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, IdenticalLists) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3), T(4), T(5)};
    std::vector<TermInfo> l2 = {T(1), T(2), T(3), T(4), T(5)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(ids(result), ids(l1));
    CheckSorted(result);
}

TEST_F(IntersectListsTest, SingleElementIntersection) {
    std::vector<TermInfo> l1 = {T(5)};
    std::vector<TermInfo> l2 = {T(5)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].doc_id, 5);
}

TEST_F(IntersectListsTest, SingleElementNoIntersection) {
    std::vector<TermInfo> l1 = {T(5)};
    std::vector<TermInfo> l2 = {T(3)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, OneListContainsOther) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3), T(4), T(5)};
    std::vector<TermInfo> l2 = {T(2), T(3)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].doc_id, 2);
    EXPECT_EQ(result[1].doc_id, 3);
    CheckSorted(result);
}

TEST_F(IntersectListsTest, IntersectionAtStart) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3)};
    std::vector<TermInfo> l2 = {T(1), T(2), T(7), T(8)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].doc_id, 1);
    EXPECT_EQ(result[1].doc_id, 2);
}

TEST_F(IntersectListsTest, IntersectionAtEnd) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(8), T(9)};
    std::vector<TermInfo> l2 = {T(3), T(4), T(8), T(9)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].doc_id, 8);
    EXPECT_EQ(result[1].doc_id, 9);
}

TEST_F(IntersectListsTest, IntersectionInMiddle) {
    std::vector<TermInfo> l1 = {T(1), T(5), T(6), T(7), T(10)};
    std::vector<TermInfo> l2 = {T(2), T(5), T(6), T(8)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].doc_id, 5);
    EXPECT_EQ(result[1].doc_id, 6);
}

TEST_F(IntersectListsTest, LargeListsWithFullIntersection) {
    auto l1 = make_range(0, 1000);   // 0, 1, 2, ..., 999
    auto l2 = make_range(250, 500);  // 250, 251, ..., 749

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 500);
    EXPECT_EQ(result.front().doc_id, 250);
    EXPECT_EQ(result.back().doc_id, 749);
    CheckSorted(result);
}

TEST_F(IntersectListsTest, LargeListsWithSparseIntersection) {
    std::vector<TermInfo> l1 = {T(0), T(10), T(20), T(30), T(40), T(50), T(60), T(70), T(80), T(90)};
    std::vector<TermInfo> l2 = {T(15), T(25), T(35), T(45), T(55), T(65), T(75), T(85), T(95)};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

// Производительность
TEST_F(IntersectListsTest, PerformanceLargeListsPerformance) {
    auto l1 = make_range(0, 100000);
    auto l2 = make_range(0, 100000);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = intersect_lists(l1, l2);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(result.size(), 100000);
    EXPECT_LT(duration.count(), 100) << "Intersection too slow: " << duration.count() << "ms";
    CheckSorted(result);
}

class UnionListsTest : public ::testing::Test {
protected:
    void CheckSorted(const std::vector<TermInfo>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_LT(v[i - 1].doc_id, v[i].doc_id) << "Vector not sorted at position " << i;
        }
    }

    void CheckNoDuplicates(const std::vector<TermInfo>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_NE(v[i - 1].doc_id, v[i].doc_id) << "Duplicate found at position " << i;
        }
    }
};

TEST_F(UnionListsTest, BasicUnion) {
    std::vector<TermInfo> l1 = {T(1), T(3), T(5)};
    std::vector<TermInfo> l2 = {T(2), T(3), T(4)};

    auto result = union_lists(l1, l2);

    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result.front().doc_id, 1);
    EXPECT_EQ(result.back().doc_id, 5);
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, NoCommonElements) {
    std::vector<TermInfo> l1 = {T(1), T(3), T(5)};
    std::vector<TermInfo> l2 = {T(2), T(4), T(6)};

    auto result = union_lists(l1, l2);

    EXPECT_EQ(result.size(), 6);
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, EmptyFirstList) {
    std::vector<TermInfo> l1 = {};
    std::vector<TermInfo> l2 = {T(1), T(2), T(3)};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(ids(result), ids(l2));
    CheckSorted(result);
}

TEST_F(UnionListsTest, EmptySecondList) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3)};
    std::vector<TermInfo> l2 = {};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(ids(result), ids(l1));
    CheckSorted(result);
}

TEST_F(UnionListsTest, BothListsEmpty) {
    std::vector<TermInfo> l1 = {};
    std::vector<TermInfo> l2 = {};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(UnionListsTest, IdenticalLists) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3), T(4), T(5)};
    std::vector<TermInfo> l2 = {T(1), T(2), T(3), T(4), T(5)};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(ids(result), ids(l1));
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, SingleElement) {
    std::vector<TermInfo> l1 = {T(5)};
    std::vector<TermInfo> l2 = {T(5)};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].doc_id, 5);
}

TEST_F(UnionListsTest, OneListContainsOther) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3), T(4), T(5)};
    std::vector<TermInfo> l2 = {T(2), T(3), T(4)};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(ids(result), ids(l1));
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, OverlappingDifferentSizes) {
    std::vector<TermInfo> l1 = {T(1), T(2), T(3)};
    std::vector<TermInfo> l2 = {T(2), T(3), T(4), T(5), T(6)};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 6);
    CheckSorted(result);
    CheckNoDuplicates(result);

    auto result_ids = ids(result);
    for (int elem : {1, 2, 3, 4, 5, 6}) {
        EXPECT_TRUE(std::find(result_ids.begin(), result_ids.end(), elem) != result_ids.end());
    }
}

TEST_F(UnionListsTest, LargeListUnion) {
    auto l1 = make_range(0, 5000);     // 0..4999
    auto l2 = make_range(2500, 5000);  // 2500..7499

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 7500);  // 0..7499
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, PerformanceLargeUnion) {
    auto l1 = make_range(0, 100000);
    auto l2 = make_range(50000, 100000);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = union_lists(l1, l2);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(result.size(), 150000);
    EXPECT_LT(duration.count(), 150) << "Union too slow: " << duration.count() << "ms";
    CheckSorted(result);
    CheckNoDuplicates(result);
}

class NotListTest : public ::testing::Test {
protected:
    void CheckSorted(const std::vector<TermInfo>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_LT(v[i - 1].doc_id, v[i].doc_id) << "Vector not sorted at position " << i;
        }
    }
};

TEST_F(NotListTest, BasicNegation) {
    std::vector<TermInfo> l = {T(1), T(3), T(5)};
    int total_docs = 6;

    auto result = not_list(l, total_docs);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].doc_id, 0);
    EXPECT_EQ(result[1].doc_id, 2);
    EXPECT_EQ(result[2].doc_id, 4);
    CheckSorted(result);
}

TEST_F(NotListTest, NegateEmptyList) {
    std::vector<TermInfo> l = {};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0].doc_id, 0);
    EXPECT_EQ(result[4].doc_id, 4);
    CheckSorted(result);
}

TEST_F(NotListTest, NegateAllDocuments) {
    std::vector<TermInfo> l = {T(0), T(1), T(2), T(3), T(4)};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(NotListTest, NegateFirstElement) {
    std::vector<TermInfo> l = {T(0)};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0].doc_id, 1);
    EXPECT_EQ(result[3].doc_id, 4);
}

TEST_F(NotListTest, NegateLastElement) {
    std::vector<TermInfo> l = {T(4)};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0].doc_id, 0);
    EXPECT_EQ(result[3].doc_id, 3);
}

TEST_F(NotListTest, NegateMiddleElement) {
    std::vector<TermInfo> l = {T(2)};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 4);
    std::vector<int> expected = {0, 1, 3, 4};
    EXPECT_EQ(ids(result), expected);
}

TEST_F(NotListTest, SingleDocument) {
    std::vector<TermInfo> l = {T(0)};
    int total_docs = 1;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(NotListTest, NegateNonexistentDocument) {
    std::vector<TermInfo> l = {};
    int total_docs = 0;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(NotListTest, LargeListNegation) {
    std::vector<TermInfo> l;
    for (int i = 0; i < 10000; i += 2) {
        l.push_back(T(i));
    }
    int total_docs = 10000;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 5000);

    for (int i = 0; i < 5000; i++) {
        EXPECT_EQ(result[i].doc_id, 2 * i + 1);
    }
    CheckSorted(result);
}

TEST_F(NotListTest, PerformanceLargeNegation) {
    std::vector<TermInfo> l;
    for (int i = 0; i < 100000; i++) {
        l.push_back(T(i));
    }
    int total_docs = 100000;

    auto start = std::chrono::high_resolution_clock::now();
    auto result = not_list(l, total_docs);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(result.size(), 0);
    EXPECT_LT(duration.count(), 100) << "Negation too slow: " << duration.count() << "ms";
}

class BooleanAlgebraTest : public ::testing::Test {
protected:
    void CheckSorted(const std::vector<TermInfo>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_LT(v[i - 1].doc_id, v[i].doc_id);
        }
    }
};

TEST_F(BooleanAlgebraTest, DeMorganFirstLaw) {
    std::vector<TermInfo> A = {T(1), T(3), T(5)};
    std::vector<TermInfo> B = {T(2), T(3), T(4)};
    int total = 6;

    auto and_result = intersect_lists(A, B);
    auto left = not_list(and_result, total);

    auto not_a = not_list(A, total);
    auto not_b = not_list(B, total);
    auto right = union_lists(not_a, not_b);

    EXPECT_EQ(ids(left), ids(right));
    CheckSorted(left);
    CheckSorted(right);
}

TEST_F(BooleanAlgebraTest, DeMorganSecondLaw) {
    std::vector<TermInfo> A = {T(1), T(3), T(5)};
    std::vector<TermInfo> B = {T(2), T(3), T(4)};
    int total = 6;

    auto or_result = union_lists(A, B);
    auto left = not_list(or_result, total);

    auto not_a = not_list(A, total);
    auto not_b = not_list(B, total);
    auto right = intersect_lists(not_a, not_b);

    EXPECT_EQ(ids(left), ids(right));
    CheckSorted(left);
}

TEST_F(BooleanAlgebraTest, AbsorptionLawOR) {
    std::vector<TermInfo> A = {T(1), T(3), T(5), T(7)};
    std::vector<TermInfo> B = {T(2), T(3), T(4), T(5)};

    auto and_result = intersect_lists(A, B);
    auto result = union_lists(A, and_result);

    EXPECT_EQ(ids(result), ids(A));
}

TEST_F(BooleanAlgebraTest, AbsorptionLawAND) {
    std::vector<TermInfo> A = {T(1), T(3), T(5), T(7)};
    std::vector<TermInfo> B = {T(2), T(3), T(4), T(5)};

    auto or_result = union_lists(A, B);
    auto result = intersect_lists(A, or_result);

    EXPECT_EQ(ids(result), ids(A));
}

TEST_F(BooleanAlgebraTest, Involution) {
    std::vector<TermInfo> A = {T(1), T(3), T(5)};
    int total = 6;

    auto not_a = not_list(A, total);
    auto not_not_a = not_list(not_a, total);

    EXPECT_EQ(ids(not_not_a), ids(A));
}

class RealWorldScenariosTest : public ::testing::Test {};

TEST_F(RealWorldScenariosTest, QueryAND_B_OR_C) {
    std::vector<TermInfo> A = {T(1), T(2), T(3), T(4)};
    std::vector<TermInfo> B = {T(2), T(3), T(5), T(6)};
    std::vector<TermInfo> C = {T(7), T(8), T(9)};

    auto and_result = intersect_lists(A, B);
    auto final_result = union_lists(and_result, C);

    EXPECT_EQ(final_result.size(), 5);
    EXPECT_EQ(final_result.front().doc_id, 2);
    EXPECT_EQ(final_result.back().doc_id, 9);
}

TEST_F(RealWorldScenariosTest, QueryA_OR_B_AND_NOT_C) {
    std::vector<TermInfo> A = {T(1), T(2), T(3)};
    std::vector<TermInfo> B = {T(3), T(4), T(5)};
    std::vector<TermInfo> C = {T(2), T(4)};
    int total = 6;

    auto or_result = union_lists(A, B);                     // (A | B) = {1, 2, 3, 4, 5}
    auto not_c = not_list(C, total);                        // !C = {0, 1, 3, 5}
    auto final_result = intersect_lists(or_result, not_c);  // {1, 2, 3, 4, 5} & {0, 1, 3, 5} = {1, 3, 5}

    EXPECT_EQ(final_result.size(), 3);
    auto final_ids = ids(final_result);
    EXPECT_TRUE(std::find(final_ids.begin(), final_ids.end(), 1) != final_ids.end());
    EXPECT_TRUE(std::find(final_ids.begin(), final_ids.end(), 2) == final_ids.end());
    EXPECT_TRUE(std::find(final_ids.begin(), final_ids.end(), 4) == final_ids.end());
}

TEST_F(RealWorldScenariosTest, MultipleIntersections) {
    std::vector<TermInfo> A = {T(1), T(2), T(3), T(4), T(5)};
    std::vector<TermInfo> B = {T(2), T(3), T(4), T(5), T(6)};
    std::vector<TermInfo> C = {T(3), T(4), T(5), T(6), T(7)};

    auto ab = intersect_lists(A, B);
    auto abc = intersect_lists(ab, C);

    EXPECT_EQ(abc.size(), 3);
}

TEST_F(RealWorldScenariosTest, MultipleUnions) {
    std::vector<TermInfo> A = {T(1)};
    std::vector<TermInfo> B = {T(2)};
    std::vector<TermInfo> C = {T(3)};

    auto ab = union_lists(A, B);
    auto abc = union_lists(ab, C);

    EXPECT_EQ(abc.size(), 3);
}
