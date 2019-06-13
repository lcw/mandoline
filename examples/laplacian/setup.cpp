#include "setup.h"
void remove_noair_kernel(const mandoline::CutCellMesh<3>& ccm, Eigen::SparseMatrix<double>& M, Eigen::VectorXd& rhs) {
    M.makeCompressed();
    //pin one pressure sample to remove the 1D null space, and make truly Positive Definite
    //(in the case where there is no liquid surface).
    int del_index = -1;
    for(int i = 0; i < ccm.cells().size(); ++i) {
        if(ccm.cells()[i].region  == 0 && rhs(i) == 0) {
            del_index = i;
        }
    }
    if(del_index == -1) {
        mtao::logging::debug() << "No index to delete? no fluid perhaps?";
    } else {

        //zero the RHS
        rhs[del_index] = 0;
    }

    //replace row/col with identity
    for (Eigen::SparseMatrix<double>::InnerIterator it(M, del_index); it; ++it)
    {
        if (it.col() == it.row()) {
            M.coeffRef(it.row(), it.col()) = 1;
        }
        else {
            M.coeffRef(it.row(), it.col()) = 0;
            M.coeffRef(it.col(), it.row()) = 0;
        }
    }
}
void fix_axis_aligned_indefiniteness(Eigen::SparseMatrix<double>& M, Eigen::VectorXd& rhs) {
    M = M.pruned();
    //replace empty rows/cols to make (at least) positive semi-definite
    for (int row = 0; row < M.outerSize(); ++row) {

        int count = M.innerVector(row).nonZeros();
        if (count == 0) {
            M.insert(row, row) = 1;
            std::cout << rhs[row] << " ";
            rhs[row] = 0;
        }
    }
    std::cout << std::endl;
    M.makeCompressed();
}
Eigen::VectorXd solve_SPSD_system(const Eigen::SparseMatrix<double>& A, const Eigen::VectorXd& b) {
    Eigen::VectorXd x;
    auto solve = [&](auto&& solver) -> bool {
        solver.compute(A);
        if (solver.info() != Eigen::Success) {
            mtao::logging::warn()  << "Eigen factorization failed.\n";
            return false;
        }
        x = solver.solve(b);
        if (solver.info() != Eigen::Success) {
            mtao::logging::warn() << "Eigen solve failed.\n";
            return false;
        }
        return true;
    };

    if(!solve(Eigen::ConjugateGradient< Eigen::SparseMatrix<double> >()) ) {
            mtao::logging::warn() << "CG failed, falling back to LDLT!";
        if(!solve(Eigen::SimplicialLDLT< Eigen::SparseMatrix<double> >()) ) {
            mtao::logging::error() << "Solver failed!";
        }
    }
    return x;
}


mandoline::CutCellMesh<3> read(const std::string& filename) {
    return mandoline::CutCellMesh<3>::from_proto(filename);
}


std::map<int,int> face_regions(const mandoline::CutCellMesh<3>& ccm) {
    std::map<int,int> FR;
    for(int i = 0; i < ccm.faces().size(); ++i) {
        FR[i] = 0;
    }
    for(auto&& c: ccm.cells()){
        const int region = c.region;
        for(auto&& [f,s]: c) {
            int& r2 = FR[f];
            r2 = std::max(r2,region);
        }
    }
    return FR;
}


mtao::VecXd flux(const mandoline::CutCellMesh<3>& ccm) {
    auto FR = face_regions(ccm);
    mtao::VecXd FV = ccm.face_volumes();
    auto r = ccm.regions();
    int rcount = *std::max_element(r.begin(),r.end());
    std::vector<double> RV(rcount+1,0);
    std::copy(RV.begin(),RV.end(),std::ostream_iterator<int>(std::cout,", "));
    std::cout << std::endl;
    for(auto&& [i,f]: mtao::iterator::enumerate(ccm.faces())) {
        //std::cout << FR.at(i) << "+=" << FV(i) << std::endl;;
        //std::cout << RV[FR.at(i)] << " => ";
        if(f.is_axial_face()) {
            FV(i) = 0;
        }
        RV[FR.at(i)] += FV(i);
        //std::cout << RV[FR.at(i)] << std::endl;;
    }
    for(int i = 0; i < ccm.face_size(); ++i) {
        if(FR.find(i) != FR.end() && FR.at(i) > 0 && RV[FR.at(i)] > 0) {
            FV(i) /= (FR.at(i) % 2 == 0 ? 1 : -1) * RV[FR.at(i)];
            //FV(i) /= RV[FR.at(i)];
        } else {
            FV(i) = 0;
        }
    }
    //std::cout << FV.transpose() << std::endl;
    std::cout << "Flux rows: " << FV.rows() << std::endl;
    return FV;
}
mtao::VecXd divergence(const mandoline::CutCellMesh<3>& ccm) {
    mtao::VecXd H3 = ccm.primal_hodge3();
    H3.setZero();
    int i;
    for(i = ccm.cells().size(); i < H3.rows(); ++i) {
        if(i < ccm.cells().size() && ccm.cells()[i].region == 0) {
            H3(i) = 1;
            break;
        } else if(ccm.adaptive_grid_regions().at(i) == 0) {
            H3(i) = 1;
            break;
        }
    }
    for(int j = i+1; j < H3.rows(); ++j) {
        if(j < ccm.cells().size() &&ccm.cells()[j].region == 0) {
            H3(j) = -1;
            break;
        } else if(ccm.adaptive_grid_regions().at(j) == 0) {
            H3(j) = -1;
            break;
        }
    }
    std::cout << H3.transpose() << std::endl;
    return H3;
    return H3.asDiagonal() * boundary(ccm).transpose() * flux(ccm);
}
Eigen::SparseMatrix<double> boundary(const mandoline::CutCellMesh<3>& ccm) {
    auto B = ccm.boundary();
    return B;
}
Eigen::SparseMatrix<double> laplacian(const mandoline::CutCellMesh<3>& ccm) {
    auto B = ccm.boundary();
    mtao::VecXd DM = mtao::VecXd::Ones(ccm.face_size());
    for(auto&& [i,f]: mtao::iterator::enumerate(ccm.faces())) {
        if(f.is_mesh_face()) {
            DM(i) = 0;
        }
    }
    B = DM.asDiagonal() * B;
    DM = mtao::VecXd::Ones(ccm.cell_size());
    for(auto&& [i,c]: mtao::iterator::enumerate(ccm.cells())) {
        if(c.region > 0) {
            DM(i) = 0;
        }
    }
    B = B * DM.asDiagonal();

    mtao::VecXd DH2 = ccm.dual_hodge2();

    std::cout << "DH: ";
    std::cout << DH2.transpose() << std::endl;

    return B.transpose() * DH2.asDiagonal() * B;
}
mtao::VecXd pressure(const mandoline::CutCellMesh<3>& ccm) {
    auto L = laplacian(ccm);
    mtao::VecXd D = divergence(ccm);
    remove_noair_kernel(ccm,L,D);
    //std::cout << D.transpose() << std::endl;
    fix_axis_aligned_indefiniteness(L,D);
    //std::cout << D.transpose() << std::endl;
    D.setRandom();
    std::cout << L.rows() << "," << L.cols() << " * " << D.rows() << std::endl;
    std::cout << L << std::endl;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver(L);
    //Eigen::ConjugateGradient<Eigen::SparseMatrix<double>> solver(L);
    mtao::VecXd R = solver.solve(D);
    //mtao::VecXd R = solve_SPSD_system(L,D);
    //std::cout << "Answer norm: " << ((L * R) - D).norm() << std::endl;
    for(auto&& [i,c]: mtao::iterator::enumerate(ccm.cells())) {
        if(c.region > 0) {
            R(i) = 0;
        }
    }
    std::cout << R.transpose() << std::endl;
    return R;

    //std::vector<mtao::VecXd> eigs;
    //for(int i = 0; i < 200; ++i) {
    //    for(int j = 0; j < 50; ++j) {

    //        D = L * D;
    //        for(auto&& e: eigs) {
    //            D -= e * e.dot(D);
    //        }
    //        D /= D.norm();
    //    }
    //    eigs.push_back(D);
    //}
    return D;

    //mtao::VecXd H2 = ccm.primal_hodge2();
    //return H2.asDiagonal() * 
}
