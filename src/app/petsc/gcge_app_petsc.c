/*
 * =====================================================================================
 *
 *       Filename:  gcge_app_petsc.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2018年09月24日 09时57分13秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* external head file */
#include "gcge_app_petsc.h"

void GCGE_PETSC_BuildVecByVec(void *s_vec, void **d_vec)
{
	PetscErrorCode ierr;
    ierr = VecDuplicate((Vec)s_vec, (Vec*)d_vec);
}
void GCGE_PETSC_BuildVecByMat(void *mat, void **vec)
{
	PetscErrorCode ierr;
    ierr = MatCreateVecs((Mat)mat, NULL, (Vec*)vec);
}
void GCGE_PETSC_BuildMultiVecByMat(void *mat, void ***multi_vec, GCGE_INT n_vec, GCGE_OPS *ops)
{
	PetscErrorCode ierr;
	Vec vector;
    ierr = MatCreateVecs((Mat)mat, NULL, &vector);
    ierr = VecDuplicateVecs(vector, n_vec, (Vec**)multi_vec);
}
void GCGE_PETSC_VecFree(void **vec)
{
	PetscErrorCode ierr;
    ierr = VecDestroy((Vec*)vec);
}
void GCGE_PETSC_FreeMultiVec(void ***MultiVec, GCGE_INT n_vec, struct GCGE_OPS_ *ops)
{
	PetscErrorCode ierr;
    ierr = VecDestroyVecs(n_vec, (Vec**)MultiVec);
}

void GCGE_PETSC_VecSetRandomValue(void *vec)
{
    PetscRandom    rctx;
    PetscErrorCode ierr;
    ierr = PetscRandomCreate(PETSC_COMM_WORLD,&rctx);
    ierr = PetscRandomSetFromOptions(rctx);
    ierr = VecSetRandom((Vec)vec, rctx);
    ierr = PetscRandomDestroy(&rctx);
}
void GCGE_PETSC_MultiVecSetRandomValue(void **multi_vec, GCGE_INT start, GCGE_INT n_vec, struct GCGE_OPS_ *ops)
{
    PetscRandom    rctx;
    PetscErrorCode ierr;
    PetscInt       i;
    ierr = PetscRandomCreate(PETSC_COMM_WORLD,&rctx);
    ierr = PetscRandomSetFromOptions(rctx);
    for(i=0; i<n_vec; i++)
    {
        ierr = VecSetRandom(((Vec*)multi_vec)[i], rctx);
    }
    ierr = PetscRandomDestroy(&rctx);
}

void GCGE_PETSC_MatDotVec(void *mat, void *x, void *r)
{
    PetscErrorCode ierr = MatMult((Mat)mat, (Vec)x, (Vec)r);
}
void GCGE_PETSC_VecAxpby(GCGE_DOUBLE a, void *x, GCGE_DOUBLE b, void *y)
{
    PetscErrorCode ierr;
	ierr = VecScale((Vec)y, b);
    if(x != y)
    {
	    ierr = VecAXPY((Vec)y, a, (Vec)x);
    }
}
void GCGE_PETSC_VecInnerProd(void *x, void *y, GCGE_DOUBLE *xTy)
{
	PetscErrorCode ierr = VecDot((Vec)x, (Vec)y, xTy);
}

void GCGE_PETSC_SetOps(GCGE_OPS *ops)
{
    /* either-or */
    ops->BuildVecByVec     = GCGE_PETSC_BuildVecByVec;
    ops->BuildVecByMat     = GCGE_PETSC_BuildVecByMat;
    ops->FreeVec           = GCGE_PETSC_VecFree;

    ops->VecSetRandomValue = GCGE_PETSC_VecSetRandomValue;
    ops->MatDotVec         = GCGE_PETSC_MatDotVec;
    ops->VecAxpby          = GCGE_PETSC_VecAxpby;
    ops->VecInnerProd      = GCGE_PETSC_VecInnerProd;
    ops->VecLocalInnerProd = GCGE_PETSC_VecInnerProd;

    ops->FreeMultiVec           = GCGE_PETSC_FreeMultiVec;
    ops->BuildMultiVecByMat     = GCGE_PETSC_BuildMultiVecByMat;
    ops->MultiVecSetRandomValue = GCGE_PETSC_MultiVecSetRandomValue;
}

void GCGE_SOLVER_SetPETSCOps(GCGE_SOLVER *solver)
{
    GCGE_PETSC_SetOps(solver->ops);
}

//下面是一个对PETSC 的GCG_Solver的初始化
GCGE_SOLVER* GCGE_PETSC_Solver_Init(Mat A, Mat B, int num_eigenvalues, int argc, char* argv[])
{
    //第一步: 定义相应的ops,para以及workspace

    //创建一个solver变量
    GCGE_SOLVER *petsc_solver;
    GCGE_INT help = GCGE_SOLVER_Create(&petsc_solver, argc, argv);
    if(help)
    {
        GCGE_SOLVER_Free(&petsc_solver);
        exit(0);
    }
    if(num_eigenvalues != -1)
        petsc_solver->para->nev = num_eigenvalues;
    //设置初始值
    int nev = petsc_solver->para->nev;
    double *eval = (double *)calloc(nev, sizeof(double)); 
    petsc_solver->eval = eval;

	PetscErrorCode ierr;
    Vec *evec;
	Vec vector;
    ierr = MatCreateVecs(A, NULL, &vector);
    ierr = VecDuplicateVecs(vector, nev, &evec);

    GCGE_SOLVER_SetMatA(petsc_solver, A);
    if(B != NULL)
        GCGE_SOLVER_SetMatB(petsc_solver, B);
    GCGE_SOLVER_SetPETSCOps(petsc_solver);
    GCGE_SOLVER_SetEigenvalues(petsc_solver, eval);
    GCGE_SOLVER_SetEigenvectors(petsc_solver, (void**)evec);
    //setup and solve
    GCGE_SOLVER_Setup(petsc_solver);
    printf("Set up finish!\n");
    return petsc_solver;
}
