/*
 * =====================================================================================
 *
 *       Filename:  test_mg.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2018年12月26日 15时50分13秒
 *
 *         Author:  Li Yu (liyu@tjufe.edu.cn), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdio.h>
#include "pase_mg.h"
#include "gcge.h"
#include "pase.h"
#include "gcge_app_slepc.h"


static char help[] = "Test MultiGrid.\n";
void GetPetscMat(Mat *A, Mat *B, PetscInt n, PetscInt m);
void PETSCPrintMat(Mat A, char *name);
void PETSCPrintVec(Vec x);
void PETSCPrintBV(BV x, char *name);
/* 
 *  Description:  测试PASE_MULTIGRID
 */
int
main ( int argc, char *argv[] )
{
    /* PetscInitialize */
    SlepcInitialize(&argc,&argv,(char*)0,help);
    PetscErrorCode ierr;
    PetscMPIInt    rank;
    PetscViewer    viewer;
    MPI_Comm_rank(PETSC_COMM_WORLD, &rank);

    /* 测试矩阵声明 */
    Mat      petsc_mat_A, petsc_mat_B;
    PetscInt n = 7, m = 3, row_start, row_end, col_start, col_end;
    /* 得到一个PETSC矩阵 */
    GetPetscMat(&petsc_mat_A, &petsc_mat_B, n, m);
    HYPRE_Int idx, j, num_levels = 3, mg_coarsest_level = 2;

    //创建gcge_ops
    GCGE_OPS *gcge_ops;
    GCGE_OPS_Create(&gcge_ops);
    GCGE_SLEPC_SetOps(gcge_ops);
    GCGE_OPS_Setup(gcge_ops);
    //用gcge_ops创建pase_ops
    PASE_OPS *pase_ops;
    PASE_OPS_Create(&pase_ops, gcge_ops);

    PASE_MULTIGRID multi_grid;
    PASE_REAL convert_time = 0.0;
    PASE_REAL amg_time = 0.0;
    PASE_MULTIGRID_Create(&multi_grid, num_levels, mg_coarsest_level, 
	  (void *)petsc_mat_A, (void *)petsc_mat_B, 
	  gcge_ops, &convert_time, &amg_time);
    multi_grid->cg_p = (void***)malloc(num_levels*sizeof(void**));
    gcge_ops->MultiVecCreateByMat(&(multi_grid->cg_p[1]), 3, multi_grid->A_array[1], gcge_ops);
#if 1
    //先测试P与PT乘以单向量是否有问题
    Vec x;
    Vec y;
    gcge_ops->VecCreateByMat((void**)&x, multi_grid->A_array[0], gcge_ops);
    gcge_ops->VecCreateByMat((void**)&y, multi_grid->A_array[1], gcge_ops);
    gcge_ops->VecSetRandomValue((void*)x, gcge_ops);
    gcge_ops->VecSetRandomValue((void*)y, gcge_ops);
    gcge_ops->MatDotVec(multi_grid->P_array[0], (void*)y, (void*)x, gcge_ops);
    gcge_ops->MatTransposeDotVec(multi_grid->P_array[0], (void*)x, (void*)y, gcge_ops);
    gcge_ops->VecDestroy((void**)&x, gcge_ops);
    gcge_ops->VecDestroy((void**)&y, gcge_ops);
#endif

    PETSCPrintMat((Mat)(multi_grid->P_array[0]), "P0");
    PETSCPrintMat((Mat)(multi_grid->P_array[1]), "P1");
    PETSCPrintMat((Mat)(multi_grid->A_array[0]), "A0");
    PETSCPrintMat((Mat)(multi_grid->A_array[1]), "A1");
    PETSCPrintMat((Mat)(multi_grid->B_array[0]), "B0");
    PETSCPrintMat((Mat)(multi_grid->B_array[1]), "B1");
    //LSSC4上View类的函数都有点问题，包括MatView, BVView, KSPView
    //MatView((Mat)(multi_grid->P_array[0]), viewer);
    //MatView((Mat)(multi_grid->A_array[1]), viewer);
#if 0
    /* 打印各层A, B, P矩阵*/
    //连续打会打不出来，不知道为什么？？？？只打一个可以
    PetscPrintf(PETSC_COMM_WORLD, "A_array\n");
    for (idx = 0; idx < num_levels; ++idx)
    {
        PetscPrintf(PETSC_COMM_WORLD, "idx = %d\n", idx);
        MatView((Mat)(multi_grid->A_array[idx]), viewer);
    }
    PetscPrintf(PETSC_COMM_WORLD, "B_array\n");
    for (idx = 0; idx < num_levels-1; ++idx)
    {
        PetscPrintf(PETSC_COMM_WORLD, "idx = %d\n", idx);
        MatView((Mat)(multi_grid->B_array[idx]), viewer);
    }
    PetscPrintf(PETSC_COMM_WORLD, "P_array\n");
    for (idx = 0; idx < num_levels-1; ++idx)
    {
        PetscPrintf(PETSC_COMM_WORLD, "idx = %d\n", idx);
        MatView((Mat)(multi_grid->P_array[idx]), viewer);
    }

#endif
    /* TODO: 测试BV结构的向量进行投影插值 */
    int level_i = 0;
    int level_j = 2;
    int num_vecs = 3;
    BV vecs_i;
    BV vecs_j;
    gcge_ops->MultiVecCreateByMat((void***)(&vecs_i), num_vecs, multi_grid->A_array[level_i], gcge_ops);
    gcge_ops->MultiVecCreateByMat((void***)(&vecs_j), num_vecs, multi_grid->A_array[level_j], gcge_ops);
    gcge_ops->MultiVecSetRandomValue((void**)vecs_i, 0, num_vecs, gcge_ops);
    gcge_ops->MultiVecSetRandomValue((void**)vecs_j, 0, num_vecs, gcge_ops);

    int mv_s[2];
    int mv_e[2];
    mv_s[0] = 0;
    mv_e[0] = num_vecs;
    mv_s[1] = 0;
    mv_e[1] = num_vecs;
    //gcge_ops->MatDotMultiVec(multi_grid->P_array[0], (void**)vecs_j, 
    //	  (void**)vecs_i, mv_s, mv_e, gcge_ops);
    //gcge_ops->MatTransposeDotMultiVec(multi_grid->P_array[0], (void**)vecs_i, 
    //	  (void**)vecs_j, mv_s, mv_e, gcge_ops);
    PASE_MULTIGRID_FromItoJ(multi_grid, level_j, level_i, mv_s, mv_e, 
            (void**)vecs_j, (void**)vecs_i);
    PETSCPrintBV(vecs_i, "Pto");
    PETSCPrintBV(vecs_j, "Pfrom");
    PASE_MULTIGRID_FromItoJ(multi_grid, level_i, level_j, mv_s, mv_e, 
            (void**)vecs_i, (void**)vecs_j);
    PETSCPrintBV(vecs_i, "Rfrom");
    PETSCPrintBV(vecs_j, "Rto");

    //GCGE_Printf("vecs_i\n");
    //BVView(vecs_i, viewer);
    //GCGE_Printf("vecs_j\n");
    //BVView(vecs_j, viewer);

    gcge_ops->MultiVecDestroy((void***)(&vecs_i), num_vecs, gcge_ops);
    gcge_ops->MultiVecDestroy((void***)(&vecs_j), num_vecs, gcge_ops);

    //注释掉Destroy不报错memory access out of range, 说明是Destroy时用错
    gcge_ops->MultiVecDestroy(&(multi_grid->cg_p[1]), 3, gcge_ops);
    error = PASE_MULTIGRID_Destroy(&multi_grid);

    ierr = MatDestroy(&petsc_mat_A);
    ierr = MatDestroy(&petsc_mat_B);

    PASE_OPS_Free(&pase_ops); 
    GCGE_OPS_Free(&gcge_ops);
    /* PetscFinalize */
    ierr = SlepcFinalize();
    return 0;
}

void GetPetscMat(Mat *A, Mat *B, PetscInt n, PetscInt m)
{
    PetscInt N = n*m;
    PetscInt Istart, Iend, II, i, j;
    PetscErrorCode ierr;
    ierr = MatCreate(PETSC_COMM_WORLD,A);
    ierr = MatSetSizes(*A,PETSC_DECIDE,PETSC_DECIDE,N,N);
    ierr = MatSetFromOptions(*A);
    ierr = MatSetUp(*A);
    ierr = MatGetOwnershipRange(*A,&Istart,&Iend);
    for (II=Istart;II<Iend;II++) {
      i = II/n; j = II-i*n;
      if (i>0) { ierr = MatSetValue(*A,II,II-n,-1.0,INSERT_VALUES); }
      if (i<m-1) { ierr = MatSetValue(*A,II,II+n,-1.0,INSERT_VALUES); }
      if (j>0) { ierr = MatSetValue(*A,II,II-1,-1.0,INSERT_VALUES); }
      if (j<n-1) { ierr = MatSetValue(*A,II,II+1,-1.0,INSERT_VALUES); }
      ierr = MatSetValue(*A,II,II,4.0,INSERT_VALUES);
    }
    ierr = MatAssemblyBegin(*A,MAT_FINAL_ASSEMBLY);
    ierr = MatAssemblyEnd(*A,MAT_FINAL_ASSEMBLY);

    ierr = MatCreate(PETSC_COMM_WORLD,B);
    ierr = MatSetSizes(*B,PETSC_DECIDE,PETSC_DECIDE,N,N);
    ierr = MatSetFromOptions(*B);
    ierr = MatSetUp(*B);
    ierr = MatGetOwnershipRange(*B,&Istart,&Iend);
    for (II=Istart;II<Iend;II++) {
      ierr = MatSetValue(*B,II,II,1.0,INSERT_VALUES);
    }
    ierr = MatAssemblyBegin(*B,MAT_FINAL_ASSEMBLY);
    ierr = MatAssemblyEnd(*B,MAT_FINAL_ASSEMBLY);

}

void PETSCPrintMat(Mat A, char *name)
{
    PetscErrorCode ierr;
    int nrows = 0, ncols = 0;
    ierr = MatGetSize(A, &nrows, &ncols);
    int row = 0, col = 0;
    const PetscInt *cols;
    const PetscScalar *vals;
    for(row=0; row<nrows; row++) {
        ierr = MatGetRow(A, row, &ncols, &cols, &vals);
        for(col=0; col<ncols; col++) {
            GCGE_Printf("%s(%d, %d) = %18.15e;\n", name, row+1, cols[col]+1, vals[col]);
        }
        ierr = MatRestoreRow(A, row, &ncols, &cols, &vals);
    }
}

void PETSCPrintVec(Vec x)
{
    PetscErrorCode ierr;
    int size = 0;
    int i = 0;
    ierr = VecGetSize(x, &size);
    const PetscScalar *array;
    ierr = VecGetArrayRead(x, &array);
    for(i=0; i<size; i++)
    {
        GCGE_Printf("%18.15e\t", array[i]);
    }
    ierr = VecRestoreArrayRead(x, &array);
    GCGE_Printf("\n");
}

void PETSCPrintBV(BV x, char *name)
{
    PetscErrorCode ierr;
    int n = 0, i = 0;
    ierr = BVGetSizes(x, NULL, NULL, &n);
    Vec xs;
    GCGE_Printf("%s = [\n", name);
    for(i=0; i<n; i++)
    {
        ierr = BVGetColumn(x, i, &xs);
	PETSCPrintVec(xs);
        ierr = BVRestoreColumn(x, i, &xs);
    }
    GCGE_Printf("];\n");
}