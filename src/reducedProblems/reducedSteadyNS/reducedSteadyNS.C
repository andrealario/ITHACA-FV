/*---------------------------------------------------------------------------*\
     ██╗████████╗██╗  ██╗ █████╗  ██████╗ █████╗       ███████╗██╗   ██╗
     ██║╚══██╔══╝██║  ██║██╔══██╗██╔════╝██╔══██╗      ██╔════╝██║   ██║
     ██║   ██║   ███████║███████║██║     ███████║█████╗█████╗  ██║   ██║
     ██║   ██║   ██╔══██║██╔══██║██║     ██╔══██║╚════╝██╔══╝  ╚██╗ ██╔╝
     ██║   ██║   ██║  ██║██║  ██║╚██████╗██║  ██║      ██║      ╚████╔╝
     ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝      ╚═╝       ╚═══╝

 * In real Time Highly Advanced Computational Applications for Finite Volumes
 * Copyright (C) 2017 by the ITHACA-FV authors
-------------------------------------------------------------------------------

License
    This file is part of ITHACA-FV

    ITHACA-FV is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ITHACA-FV is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with ITHACA-FV. If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

/// \file
/// Source file of the reducedSteadyNS class

#include "reducedSteadyNS.H"

// * * * * * * * * * * * * * * * Constructors * * * * * * * * * * * * * * * * //

// Constructor
reducedSteadyNS::reducedSteadyNS()
{
}

reducedSteadyNS::reducedSteadyNS(steadyNS& FOMproblem)
    :
    problem(&FOMproblem)
{
    N_BC = problem->inletIndex.rows();
    Nphi_u = problem->B_matrix.rows();
    Nphi_p = problem->K_matrix.cols();

    for (label k = 0; k < problem->liftfield.size(); k++)
    {
        Umodes.append(problem->liftfield[k]);
    }
    for (label k = 0; k < problem->NUmodes; k++)
    {
        Umodes.append(problem->Umodes[k]);
    }
    for (label k = 0; k < problem->NSUPmodes; k++)
    {
        Umodes.append(problem->supmodes[k]);
    }
    newton_object = newton_steadyNS(Nphi_u + Nphi_p , Nphi_u + Nphi_p, FOMproblem);
}

int newton_steadyNS::operator()(const Eigen::VectorXd &x, Eigen::VectorXd &fvec) const
{
    Eigen::VectorXd a_tmp(Nphi_u);
    Eigen::VectorXd b_tmp(Nphi_p);
    a_tmp = x.head(Nphi_u);
    b_tmp = x.tail(Nphi_p);
    // Convective term
    Eigen::MatrixXd cc(1, 1);
    // Mom Term
    Eigen::VectorXd M1 = problem->B_matrix * a_tmp * nu;
    // Gradient of pressure
    Eigen::VectorXd M2 = problem->K_matrix * b_tmp;
    // Pressure Term
    Eigen::VectorXd M3 = problem->P_matrix * a_tmp;

    for (label i = 0; i < Nphi_u; i++)
    {
        cc = a_tmp.transpose() * problem->C_matrix[i] * a_tmp;
        fvec(i) = M1(i) - cc(0, 0) - M2(i);
    }
    for (label j = 0; j < Nphi_p; j++)
    {
        label k = j + Nphi_u;
        fvec(k) = M3(j);
    }
    for (label j = 0; j < N_BC; j++)
    {
        fvec(j) = x(j) - BC(j);
    }
    return 0;
}


int newton_steadyNS::df(const Eigen::VectorXd &x,  Eigen::MatrixXd &fjac) const
{
    Eigen::NumericalDiff<newton_steadyNS> numDiff(*this);
    numDiff.df(x, fjac);
    return 0;
}


// * * * * * * * * * * * * * * * Solve Functions  * * * * * * * * * * * * * //

void reducedSteadyNS::solveOnline_PPE(Eigen::MatrixXd vel_now)
{
    Info << "This function is still not implemented for the stationary case" << endl;
    exit(0);
}

void reducedSteadyNS::solveOnline_sup(Eigen::MatrixXd vel_now)
{

    y.resize(Nphi_u + Nphi_p, 1);
    y.setZero();

    for (label j = 0; j < N_BC; j++)
    {
        y(j) = vel_now(j, 0);
    }
    Color::Modifier red(Color::FG_RED);
    Color::Modifier green(Color::FG_GREEN);
    Color::Modifier def(Color::FG_DEFAULT);
    Eigen::HybridNonLinearSolver<newton_steadyNS> hnls(newton_object);
    newton_object.BC.resize(N_BC);
    for (label j = 0; j < N_BC; j++)
    {
        newton_object.BC(j) = vel_now(j, 0);
    }
    newton_object.nu = nu;
    hnls.solve(y);

    Eigen::VectorXd res(y);
    newton_object.operator()(y, res);

    std::cout << "################## Online solve N° " << count_online_solve << " ##################" << std::endl;
    std::cout << "Solving for the parameter: " << vel_now << std::endl;
    if (res.norm() < 1e-5)
    {
        std::cout << green << "|F(x)| = " << res.norm() << " - Minimun reached in " << hnls.iter << " iterations " << def << std::endl << std::endl;
    }
    else
    {
        std::cout << red << "|F(x)| = " << res.norm() << " - Minimun reached in " << hnls.iter << " iterations " << def << std::endl << std::endl;
    }
    count_online_solve += 1;
}


// * * * * * * * * * * * * * * * Jacobian Evaluation  * * * * * * * * * * * * * //

void reducedSteadyNS::reconstruct_PPE(fileName folder, int printevery)
{
    mkDir(folder);
    ITHACAutilities::createSymLink(folder);

    int counter = 0;
    int nextwrite = 0;

    for (label i = 0; i < online_solution.size(); i++)
    {
        if (counter == nextwrite)
        {
            volVectorField U_rec("U_rec", Umodes[0] * 0);
            for (label j = 0; j < Nphi_u; j++)
            {
                U_rec += Umodes[j] * online_solution[i](j + 1, 0);
            }
            problem->exportSolution(U_rec, name(online_solution[i](0, 0)), folder);
            volScalarField P_rec("P_rec", problem->Pmodes[0] * 0);
            for (label j = 0; j < Nphi_p; j++)
            {
                P_rec += problem->Pmodes[j] * online_solution[i](j + Nphi_u + 1, 0);
            }
            problem->exportSolution(P_rec, name(online_solution[i](0, 0)), folder);
            nextwrite += printevery;
        }
        counter++;
    }
}

void reducedSteadyNS::reconstruct_sup(fileName folder, int printevery)
{
    mkDir(folder);
    ITHACAutilities::createSymLink(folder);

    int counter = 0;
    int nextwrite = 0;

    for (label i = 0; i < online_solution.size(); i++)
    {
        if (counter == nextwrite)
        {
            volVectorField U_rec("U_rec", Umodes[0] * 0);
            for (label j = 0; j < Nphi_u; j++)
            {
                U_rec += Umodes[j] * online_solution[i](j + 1, 0);
            }
            problem->exportSolution(U_rec, name(online_solution[i](0, 0)), folder);
            volScalarField P_rec("P_rec", problem->Pmodes[0] * 0);
            for (label j = 0; j < Nphi_p; j++)
            {
                P_rec += problem->Pmodes[j] * online_solution[i](j + Nphi_u + 1, 0);
            }
            problem->exportSolution(P_rec, name(online_solution[i](0, 0)), folder);
            nextwrite += printevery;

            UREC.append(U_rec);
            PREC.append(P_rec);
        }
        counter++;
    }
}

double reducedSteadyNS::inf_sup_constant()
{
    double a;
    Eigen::VectorXd sup(Nphi_u);
    Eigen::VectorXd inf(Nphi_p);
    for (int i = 0; i < Nphi_p; i++)
    {
        for (int j = 0; j < Nphi_u; j++)
        {
            sup(j) = fvc::domainIntegrate(fvc::div(Umodes[j]) * Pmodes[i]).value() / ITHACAutilities::H1seminorm(Umodes[j]) / ITHACAutilities::L2norm(Pmodes[i]);
        }
        inf(i) = sup.maxCoeff();
    }
    a = inf.minCoeff();
    return a;
}


void reducedSteadyNS::reconstruct_LiftandDrag(steadyNS & problem, fileName folder)
{
    mkDir(folder);
    system("ln -s ../../constant " + folder + "/constant");
    system("ln -s ../../0 " + folder + "/0");
    system("ln -s ../../system " + folder + "/system");

    label NUmodes = problem.NUmodes;
    label NSUPmodes = problem.NSUPmodes;
    label NPmodes = problem.NPmodes;
    label liftfieldSize = problem.liftfield.size();
    label totalSize = NUmodes + NSUPmodes + liftfieldSize;

    Eigen::VectorXd cl(online_solution.size());
    Eigen::VectorXd cd(online_solution.size());

    //Read FORCESdict
    IOdictionary FORCESdict
    (
        IOobject
        (
            "FORCESdict",
            "./system",
            Umodes[0].mesh(),
            IOobject::MUST_READ,
            IOobject::NO_WRITE
        )
    );

    Eigen::MatrixXd TAU = ITHACAstream::readMatrix("./ITHACAoutput/Matrices/tau_mat.txt");
    Eigen::MatrixXd N = ITHACAstream::readMatrix("./ITHACAoutput/Matrices/n_mat.txt");
    Eigen::VectorXd temp1;

    f_tau.setZero(online_solution.size(), 3);
    f_n.setZero(online_solution.size(), 3);

    for (label i = 0; i < online_solution.size(); i++)
    {

        for (label j = 0; j < totalSize; j++)
        {
            f_tau.row(i) += TAU.row(j) * online_solution[i](j + 1, 0);
        }

        for (label j = 0; j < NPmodes; j++)
        {
            f_n.row(i) += N.row(j) * online_solution[i](j + Nphi_u + 1, 0);
        }
    }

    ITHACAstream::exportMatrix(f_tau, "f_tau", "python", folder);
    ITHACAstream::exportMatrix(f_tau, "f_tau", "matlab", folder);
    ITHACAstream::exportMatrix(f_tau, "f_tau", "eigen", folder);
    ITHACAstream::exportMatrix(f_n, "f_n", "python", folder);
    ITHACAstream::exportMatrix(f_n, "f_n", "matlab", folder);
    ITHACAstream::exportMatrix(f_n, "f_n", "eigen", folder);
}

