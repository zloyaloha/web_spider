#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <numeric>

#include "searcher.h"

class IntersectListsTest : public ::testing::Test {
protected:
    void CheckSorted(const std::vector<int>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_LT(v[i - 1], v[i]) << "Vector not sorted at position " << i;
        }
    }
};

TEST_F(IntersectListsTest, BasicIntersection) {
    std::vector<int> l1 = {1, 3, 5, 7};
    std::vector<int> l2 = {3, 5, 9};

    auto result = intersect_lists(l1, l2);

    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 3);
    EXPECT_EQ(result[1], 5);
    CheckSorted(result);
}

TEST_F(IntersectListsTest, NoIntersection) {
    std::vector<int> l1 = {1, 2, 3};
    std::vector<int> l2 = {4, 5, 6};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, EmptyFirstList) {
    std::vector<int> l1 = {};
    std::vector<int> l2 = {1, 2, 3};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, EmptySecondList) {
    std::vector<int> l1 = {1, 2, 3};
    std::vector<int> l2 = {};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, BothListsEmpty) {
    std::vector<int> l1 = {};
    std::vector<int> l2 = {};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, IdenticalLists) {
    std::vector<int> l1 = {1, 2, 3, 4, 5};
    std::vector<int> l2 = {1, 2, 3, 4, 5};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result, l1);
    CheckSorted(result);
}

TEST_F(IntersectListsTest, SingleElementIntersection) {
    std::vector<int> l1 = {5};
    std::vector<int> l2 = {5};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 5);
}

TEST_F(IntersectListsTest, SingleElementNoIntersection) {
    std::vector<int> l1 = {5};
    std::vector<int> l2 = {3};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(IntersectListsTest, OneListContainsOther) {
    std::vector<int> l1 = {1, 2, 3, 4, 5};
    std::vector<int> l2 = {2, 3};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 3);
    CheckSorted(result);
}

TEST_F(IntersectListsTest, IntersectionAtStart) {
    std::vector<int> l1 = {1, 2, 3};
    std::vector<int> l2 = {1, 2, 7, 8};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
}

TEST_F(IntersectListsTest, IntersectionAtEnd) {
    std::vector<int> l1 = {1, 2, 8, 9};
    std::vector<int> l2 = {3, 4, 8, 9};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 8);
    EXPECT_EQ(result[1], 9);
}

TEST_F(IntersectListsTest, IntersectionInMiddle) {
    std::vector<int> l1 = {1, 5, 6, 7, 10};
    std::vector<int> l2 = {2, 5, 6, 8};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 5);
    EXPECT_EQ(result[1], 6);
}

TEST_F(IntersectListsTest, LargeListsWithFullIntersection) {
    std::vector<int> l1(1000);
    std::iota(l1.begin(), l1.end(), 0);  // 0, 1, 2, ..., 999

    std::vector<int> l2(500);
    std::iota(l2.begin(), l2.end(), 250);  // 250, 251, ..., 749

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 500);
    EXPECT_EQ(result[0], 250);
    EXPECT_EQ(result[499], 749);
    CheckSorted(result);
}

TEST_F(IntersectListsTest, LargeListsWithSparseIntersection) {
    std::vector<int> l1 = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    std::vector<int> l2 = {15, 25, 35, 45, 55, 65, 75, 85, 95};

    auto result = intersect_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

// Производительность
TEST_F(IntersectListsTest, PerformanceLargeListsPerformance) {
    std::vector<int> l1(100000);
    std::iota(l1.begin(), l1.end(), 0);

    std::vector<int> l2(100000);
    std::iota(l2.begin(), l2.end(), 0);

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
    void CheckSorted(const std::vector<int>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_LT(v[i - 1], v[i]) << "Vector not sorted at position " << i;
        }
    }

    void CheckNoDuplicates(const std::vector<int>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_NE(v[i - 1], v[i]) << "Duplicate found at position " << i;
        }
    }
};

TEST_F(UnionListsTest, BasicUnion) {
    std::vector<int> l1 = {1, 3, 5};
    std::vector<int> l2 = {2, 3, 4};

    auto result = union_lists(l1, l2);

    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[4], 5);
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, NoCommonElements) {
    std::vector<int> l1 = {1, 3, 5};
    std::vector<int> l2 = {2, 4, 6};

    auto result = union_lists(l1, l2);

    EXPECT_EQ(result.size(), 6);
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, EmptyFirstList) {
    std::vector<int> l1 = {};
    std::vector<int> l2 = {1, 2, 3};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result, l2);
    CheckSorted(result);
}

TEST_F(UnionListsTest, EmptySecondList) {
    std::vector<int> l1 = {1, 2, 3};
    std::vector<int> l2 = {};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result, l1);
    CheckSorted(result);
}

TEST_F(UnionListsTest, BothListsEmpty) {
    std::vector<int> l1 = {};
    std::vector<int> l2 = {};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(UnionListsTest, IdenticalLists) {
    std::vector<int> l1 = {1, 2, 3, 4, 5};
    std::vector<int> l2 = {1, 2, 3, 4, 5};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result, l1);
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, SingleElement) {
    std::vector<int> l1 = {5};
    std::vector<int> l2 = {5};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 5);
}

TEST_F(UnionListsTest, OneListContainsOther) {
    std::vector<int> l1 = {1, 2, 3, 4, 5};
    std::vector<int> l2 = {2, 3, 4};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result, l1);
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, OverlappingDifferentSizes) {
    std::vector<int> l1 = {1, 2, 3};
    std::vector<int> l2 = {2, 3, 4, 5, 6};

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 6);
    CheckSorted(result);
    CheckNoDuplicates(result);

    for (int elem : {1, 2, 3, 4, 5, 6}) {
        EXPECT_TRUE(std::find(result.begin(), result.end(), elem) != result.end());
    }
}

TEST_F(UnionListsTest, LargeListUnion) {
    std::vector<int> l1(5000);
    std::iota(l1.begin(), l1.end(), 0);  // 0..4999

    std::vector<int> l2(5000);
    std::iota(l2.begin(), l2.end(), 2500);  // 2500..7499

    auto result = union_lists(l1, l2);
    EXPECT_EQ(result.size(), 7500);  // 0..7499
    CheckSorted(result);
    CheckNoDuplicates(result);
}

TEST_F(UnionListsTest, PerformanceLargeUnion) {
    std::vector<int> l1(100000);
    std::iota(l1.begin(), l1.end(), 0);

    std::vector<int> l2(100000);
    std::iota(l2.begin(), l2.end(), 50000);

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
    void CheckSorted(const std::vector<int>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_LT(v[i - 1], v[i]) << "Vector not sorted at position " << i;
        }
    }
};

TEST_F(NotListTest, BasicNegation) {
    std::vector<int> l = {1, 3, 5};
    int total_docs = 6;

    auto result = not_list(l, total_docs);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 4);
    CheckSorted(result);
}

TEST_F(NotListTest, NegateEmptyList) {
    std::vector<int> l = {};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[4], 4);
    CheckSorted(result);
}

TEST_F(NotListTest, NegateAllDocuments) {
    std::vector<int> l = {0, 1, 2, 3, 4};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(NotListTest, NegateFirstElement) {
    std::vector<int> l = {0};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[3], 4);
}

TEST_F(NotListTest, NegateLastElement) {
    std::vector<int> l = {4};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[3], 3);
}

TEST_F(NotListTest, NegateMiddleElement) {
    std::vector<int> l = {2};
    int total_docs = 5;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 4);
    std::vector<int> expected = {0, 1, 3, 4};
    EXPECT_EQ(result, expected);
}

TEST_F(NotListTest, SingleDocument) {
    std::vector<int> l = {0};
    int total_docs = 1;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(NotListTest, NegateNonexistentDocument) {
    std::vector<int> l = {};
    int total_docs = 0;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(NotListTest, LargeListNegation) {
    std::vector<int> l;
    for (int i = 0; i < 10000; i += 2) {
        l.push_back(i);
    }
    int total_docs = 10000;

    auto result = not_list(l, total_docs);
    EXPECT_EQ(result.size(), 5000);

    for (int i = 0; i < 5000; i++) {
        EXPECT_EQ(result[i], 2 * i + 1);
    }
    CheckSorted(result);
}

TEST_F(NotListTest, PerformanceLargeNegation) {
    std::vector<int> l;
    for (int i = 0; i < 100000; i++) {
        l.push_back(i);
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
    void CheckSorted(const std::vector<int>& v) {
        for (size_t i = 1; i < v.size(); i++) {
            EXPECT_LT(v[i - 1], v[i]);
        }
    }
};

TEST_F(BooleanAlgebraTest, DeMorganFirstLaw) {
    std::vector<int> A = {1, 3, 5};
    std::vector<int> B = {2, 3, 4};
    int total = 6;

    auto and_result = intersect_lists(A, B);
    auto left = not_list(and_result, total);

    auto not_a = not_list(A, total);
    auto not_b = not_list(B, total);
    auto right = union_lists(not_a, not_b);

    EXPECT_EQ(left, right);
    CheckSorted(left);
    CheckSorted(right);
}

TEST_F(BooleanAlgebraTest, DeMorganSecondLaw) {
    std::vector<int> A = {1, 3, 5};
    std::vector<int> B = {2, 3, 4};
    int total = 6;

    auto or_result = union_lists(A, B);
    auto left = not_list(or_result, total);

    auto not_a = not_list(A, total);
    auto not_b = not_list(B, total);
    auto right = intersect_lists(not_a, not_b);

    EXPECT_EQ(left, right);
    CheckSorted(left);
}

TEST_F(BooleanAlgebraTest, AbsorptionLawOR) {
    std::vector<int> A = {1, 3, 5, 7};
    std::vector<int> B = {2, 3, 4, 5};

    auto and_result = intersect_lists(A, B);
    auto result = union_lists(A, and_result);

    EXPECT_EQ(result, A);
}

TEST_F(BooleanAlgebraTest, AbsorptionLawAND) {
    std::vector<int> A = {1, 3, 5, 7};
    std::vector<int> B = {2, 3, 4, 5};

    auto or_result = union_lists(A, B);
    auto result = intersect_lists(A, or_result);

    EXPECT_EQ(result, A);
}

TEST_F(BooleanAlgebraTest, Involution) {
    std::vector<int> A = {1, 3, 5};
    int total = 6;

    auto not_a = not_list(A, total);
    auto not_not_a = not_list(not_a, total);

    EXPECT_EQ(not_not_a, A);
}

class RealWorldScenariosTest : public ::testing::Test {};

TEST_F(RealWorldScenariosTest, QueryAND_B_OR_C) {
    std::vector<int> A = {1, 2, 3, 4};
    std::vector<int> B = {2, 3, 5, 6};
    std::vector<int> C = {7, 8, 9};

    auto and_result = intersect_lists(A, B);
    auto final_result = union_lists(and_result, C);

    EXPECT_EQ(final_result.size(), 5);
    EXPECT_EQ(final_result[0], 2);
    EXPECT_EQ(final_result[4], 9);
}

TEST_F(RealWorldScenariosTest, QueryA_OR_B_AND_NOT_C) {
    std::vector<int> A = {1, 2, 3};
    std::vector<int> B = {3, 4, 5};
    std::vector<int> C = {2, 4};
    int total = 6;

    auto or_result = union_lists(A, B);                     // (A | B) = {1, 2, 3, 4, 5}
    auto not_c = not_list(C, total);                        // !C = {0, 1, 3, 5}
    auto final_result = intersect_lists(or_result, not_c);  // {1, 2, 3, 4, 5} & {0, 1, 3, 5} = {1, 3, 5}

    EXPECT_EQ(final_result.size(), 3);
    EXPECT_TRUE(std::find(final_result.begin(), final_result.end(), 1) != final_result.end());
    EXPECT_TRUE(std::find(final_result.begin(), final_result.end(), 2) == final_result.end());
    EXPECT_TRUE(std::find(final_result.begin(), final_result.end(), 4) == final_result.end());
}

TEST_F(RealWorldScenariosTest, MultipleIntersections) {
    std::vector<int> A = {1, 2, 3, 4, 5};
    std::vector<int> B = {2, 3, 4, 5, 6};
    std::vector<int> C = {3, 4, 5, 6, 7};

    auto ab = intersect_lists(A, B);
    auto abc = intersect_lists(ab, C);

    EXPECT_EQ(abc.size(), 3);
}

TEST_F(RealWorldScenariosTest, MultipleUnions) {
    std::vector<int> A = {1};
    std::vector<int> B = {2};
    std::vector<int> C = {3};

    auto ab = union_lists(A, B);
    auto abc = union_lists(ab, C);

    EXPECT_EQ(abc.size(), 3);
}
