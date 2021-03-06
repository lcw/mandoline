#include <iostream>
#include <mandoline/construction/face_collapser.hpp>
#include <catch2/catch.hpp>
#include <iterator>
#include <mandoline/tools/edges_to_plcurves.hpp>

using E = std::array<int, 2>;
using namespace mandoline::construction;

TEST_CASE("Polygon", "[face_collapser]") {
    for (int N = 3; N < 50; ++N) {
        std::stringstream ss;
        ss << "Polygon size " << N;
        SECTION(ss.str());
        mtao::ColVecs2d V(2, N);

        mtao::VecXd ang = mtao::VecXd::LinSpaced(N, 0, 2 * M_PI * (N - 1.) / N);
        V.row(0) = ang.array().cos().transpose();
        V.row(1) = ang.array().sin().transpose();


        std::set<E> edges;
        for (int j = 0; j < N; ++j) {
            edges.emplace(E{ { j, (j + 1) % N } });
        }


        FaceCollapser fc(edges);
        fc.bake(V, false);


        {
            auto faces = fc.faces_no_holes();
            REQUIRE(faces.size() == 2);
            {
                auto prit = faces.find(0);
                REQUIRE(prit != faces.end());
                REQUIRE(prit->second.size() == N);
            }
            {
                auto prit = faces.find(1);
                REQUIRE(prit != faces.end());
                REQUIRE(prit->second.size() == N);
            }
        }

        fc = FaceCollapser(edges);
        fc.set_edge_for_removal(E{ { 1, 0 } });
        fc.bake(V, false);

        {
            auto faces = fc.faces_no_holes();
            REQUIRE(faces.size() == 1);
            {
                auto prit = faces.find(0);
                REQUIRE(prit != faces.end());
                REQUIRE(prit->second.size() == N);
            }
            for (auto &&[i, v] : mtao::iterator::enumerate(faces[1])) {
                REQUIRE(i == v);
            }
        }
    }
}
TEST_CASE("Polygon with Center", "[face_collapser]") {
    for (int N = 3; N < 50; ++N) {
        std::stringstream ss;
        ss << "Polygon size " << N;
        SECTION(ss.str());
        mtao::ColVecs2d V(2, N + 1);

        mtao::VecXd ang = mtao::VecXd::LinSpaced(N, 0, 2 * M_PI * (N - 1.) / N);
        V.row(0).head(N) = ang.array().cos().transpose();
        V.row(1).head(N) = ang.array().sin().transpose();
        V.col(N).setZero();


        std::set<E> edges;
        for (int j = 0; j < N; ++j) {
            edges.emplace(E{ { j, (j + 1) % N } });
            edges.emplace(E{ { j, N } });
        }


        FaceCollapser fc(edges);
        fc.bake(V, false);


        {
            auto faces = fc.faces_no_holes();
            REQUIRE(faces.size() == N + 1);
            {
                int triangle_count = 0;
                int non_tri_poly_count = 0;
                for (auto &&[a, b] : faces) {
                    int size = b.size();
                    if (size == 3) {
                        triangle_count++;
                    } else {
                        non_tri_poly_count++;
                    }
                }
                if (N > 3) {
                    REQUIRE(triangle_count == N);
                    REQUIRE(non_tri_poly_count == 1);
                } else {
                    REQUIRE(triangle_count == N + 1);
                }
            }
        }

        fc = FaceCollapser(edges);
        fc.set_edge_for_removal(E{ { 1, 0 } });
        fc.bake(V, false);

        {
            auto faces = fc.faces_no_holes();
            REQUIRE(faces.size() == N);
            for (auto [a, l] : faces) {
                REQUIRE(l.size() == 3);
                std::sort(l.begin(), l.end());
                auto it = l.begin();
                int first = *it++;
                int second = *it++;
                int third = *it++;

                if (first == 0 && second != 1) {
                    REQUIRE(second == N - 1);
                } else {
                    REQUIRE(first + 1 == second);
                }
                REQUIRE(third == N);
            }
        }
    }
}
TEST_CASE("Near Axis Fan", "[face_collapser]") {
    for (int N = 3; N < 50; ++N) {
        std::stringstream ss;
        SECTION(ss.str());
        mtao::ColVecs2d V(2, N + 1);

        V.row(0).head(N).setConstant(1e-8);
        V.row(1).head(N) = mtao::VecXd::LinSpaced(N, 1, 1e10);
        V.col(N).setZero();


        std::set<E> edges;
        for (int j = 0; j < N; ++j) {
            if (j + 1 < N) {
                edges.emplace(E{ { j, (j + 1) } });
            }
            edges.emplace(E{ { j, N } });
        }


        FaceCollapser fc(edges);
        fc.set_edge_for_removal(E{ { 1, 0 } });
        fc.bake(V, false);

        {
            auto faces = fc.faces_no_holes();
            REQUIRE(faces.size() == N - 1);
            for (auto [a, l] : faces) {
                REQUIRE(l.size() == 3);
                std::sort(l.begin(), l.end());
                auto it = l.begin();
                int first = *it++;
                int second = *it++;
                int third = *it++;

                REQUIRE(first + 1 == second);
                REQUIRE(third == N);
            }
        }
    }
}

TEST_CASE("Near Diagonal Fan", "[face_collapser]") {
    for (int N = 3; N < 50; ++N) {
        std::stringstream ss;
        SECTION(ss.str());
        mtao::ColVecs2d V(2, N + 1);

        V.row(0).head(N) = mtao::VecXd::LinSpaced(N, 1, 1e10);
        V.row(1).head(N) = V.row(0).head(N).array() - .5;
        V.col(N).setZero();


        std::set<E> edges;
        for (int j = 0; j < N; ++j) {
            if (j + 1 < N) {
                edges.emplace(E{ { j, (j + 1) } });
            }
            edges.emplace(E{ { j, N } });
        }


        FaceCollapser fc(edges);
        fc.set_edge_for_removal(E{ { 1, 0 } });
        fc.bake(V, false);

        {
            auto faces = fc.faces_no_holes();
            REQUIRE(faces.size() == N - 1);
            for (auto [a, l] : faces) {
                REQUIRE(l.size() == 3);
                std::sort(l.begin(), l.end());
                auto it = l.begin();
                int first = *it++;
                int second = *it++;
                int third = *it++;

                REQUIRE(first + 1 == second);
                REQUIRE(third == N);
            }
        }
    }
}

TEST_CASE("Polygon with hole", "[face_collapser]") {
    // two diff sized polygons
    for (int N = 3; N < 10; ++N) {
        for (int M = 3; M < 10; ++M) {

            mtao::ColVecs2d V(2, N + M);

            // triangle in a triangle
            mtao::VecXd ang = mtao::VecXd::LinSpaced(N, 0, 2 * M_PI * (N - 1.) / N);
            V.row(0).head(N) = 3 * ang.array().cos().transpose();
            V.row(1).head(N) = 3 * ang.array().sin().transpose();
            ang = mtao::VecXd::LinSpaced(M, 0, 2 * M_PI * (M - 1.) / M);
            V.row(0).tail(M) = ang.array().cos().transpose();
            V.row(1).tail(M) = ang.array().sin().transpose();


            std::set<E> edges;
            for (int i = 0; i < N; ++i) {
                int a = i;
                int b = (i + 1) % N;
                int c = a + N;
                int d = b + N;

                edges.emplace(E{ { a, b } });
            }
            for (int i = 0; i < M; ++i) {
                int a = i;
                int b = (i + 1) % M;
                int c = a + N;
                int d = b + N;
                edges.emplace(E{ { c, d } });
            }


            FaceCollapser fc(edges);
            fc.set_edge_for_removal(E{ { 1, 0 } });
            fc.bake(V);

            {
                auto faces = fc.faces();
                REQUIRE(faces.size() == 2);
                int num_single_loops = 0;
                int num_double_loops = 0;
                for (auto [a, l] : faces) {
                    if (l.size() == 2) {// outer one
                        num_double_loops++;

                        for (auto ll : l) {
                            std::sort(ll.begin(), ll.end());
                            int min = ll[0];
                            for (auto &&[i, v] : mtao::iterator::enumerate(ll)) {
                                if (min == 0) {
                                    REQUIRE(i == v);
                                } else {
                                    REQUIRE(i + N == v);
                                }
                            }
                        }
                    } else if (l.size() == 1) {//inner one
                        num_single_loops++;
                        auto ll = *l.begin();
                        std::sort(ll.begin(), ll.end());
                        for (auto &&[i, v] : mtao::iterator::enumerate(ll)) {
                            REQUIRE(i + N == v);
                        }
                    }
                }
                REQUIRE(num_single_loops == 1);
                REQUIRE(num_double_loops == 1);
            }
        }
    }
}

TEST_CASE("EdgeToPLCurve", "[face_collapser]") {
    mtao::ColVecs2d V(2, 6);

    V.col(0) << 0, 0;
    V.col(1) << 1, 0;
    V.col(2) << 0, 1;
    V.col(3) << 0, 2;
    V.col(4) << 2, 2;
    V.col(5) << 2, 4;

    mtao::ColVecs2i E(2, 5);

    E.col(0) << 0, 1;
    E.col(1) << 1, 2;
    E.col(2) << 2, 0;
    E.col(3) << 2, 3;
    E.col(4) << 4, 5;

    {
        // only closed curves
        auto r = mandoline::tools::edge_to_plcurves(V, E, true);
        for (auto &&[curve, closedness] : r) {
            std::sort(curve.begin(), curve.end());
            CHECK(closedness == true);
            if (curve.size() == 2) {
                CHECK(curve[0] == 4);
                CHECK(curve[1] == 5);
            } else if (curve.size() == 3) {
                CHECK(curve[0] == 0);
                CHECK(curve[1] == 1);
                CHECK(curve[2] == 2);
            } else if (curve.size() == 5) {
                CHECK(curve[0] == 0);
                CHECK(curve[1] == 1);
                CHECK(curve[2] == 2);
                CHECK(curve[3] == 2);
                CHECK(curve[4] == 3);
            }
        }
    }
    std::cout << std::endl;
    {
        // only open pieces allowed
        auto r = mandoline::tools::edge_to_plcurves(V, E, false);
        for (auto &&[curve, closedness] : r) {
            std::sort(curve.begin(), curve.end());
            if (curve.size() == 2) {
                CHECK(closedness == false);
                CHECK(curve[0] == 4);
                CHECK(curve[1] == 5);
            } else if (curve.size() == 3) {
                CHECK(closedness == true);
                CHECK(curve[0] == 0);
                CHECK(curve[1] == 1);
                CHECK(curve[2] == 2);
            } else if (curve.size() == 5) {
                CHECK(closedness == false);
                CHECK(curve[0] == 0);
                CHECK(curve[1] == 1);
                CHECK(curve[2] == 2);
                CHECK(curve[3] == 2);
                CHECK(curve[4] == 3);
            }
        }
    }
    std::cout << std::endl;
    {
        E.resize(2, 3);
        E.col(0) << 0, 1;
        E.col(1) << 1, 2;
        E.col(2) << 2, 0;
        // only open pieces allowed
        std::cout << E << std::endl;
        auto r = mandoline::tools::edge_to_plcurves(V, E, false);
        std::cout << "R.size() : " << r.size() << std::endl;
        for (auto &&[curve, closedness] : r) {
            std::sort(curve.begin(), curve.end());
            std::copy(curve.begin(), curve.end(), std::ostream_iterator<int>(std::cout, ","));
            std::cout << std::endl;
            CHECK(closedness == true);
            CHECK(curve[0] == 0);
            CHECK(curve[1] == 1);
            CHECK(curve[2] == 2);
        }
    }
    std::cout << std::endl;
    {
        E.resize(2, 3);
        E.col(0) << 0, 1;
        E.col(1) << 1, 2;
        E.col(2) << 2, 3;
        // only open pieces allowed
        std::cout << E << std::endl;
        auto r = mandoline::tools::edge_to_plcurves(V, E, false);
        std::cout << "R.size() : " << r.size() << std::endl;
        for (auto &&[curve, closedness] : r) {
            std::copy(curve.begin(), curve.end(), std::ostream_iterator<int>(std::cout, ","));
            std::cout << std::endl;
            std::sort(curve.begin(), curve.end());
            CHECK(closedness == false);
            CHECK(curve[0] == 0);
            CHECK(curve[1] == 1);
            CHECK(curve[2] == 2);
        }
    }
}
