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
#include "pase_amg.h"
#include "gcge.h"
#include "gcge_cg.h"
#include "pase.h"
#include "gcge_app_slepc.h"


static char help[] = "Test MultiGrid.\n";
void GetPetscMat(Mat *A, Mat *B, PetscInt n, PetscInt m);
void PETSCPrintMat(Mat A, char *name);
void PETSCPrintVec(Vec x);
void PETSCPrintBV(BV x, char *name);
void PASEGetCommandLineInfo(PASE_INT argc, char *argv[], PASE_INT *block_size, PASE_REAL *atol, PASE_INT *nsmooth);
void PASE_BMG_TEST( PASE_MULTIGRID mg, 
               PASE_INT current_level, 
               void **rhs, void **sol, 
               PASE_INT *start, PASE_INT *end,
               PASE_REAL tol, PASE_REAL rate, 
               PASE_INT nsmooth, PASE_INT max_coarest_nsmooth);
void PETSC_Sub_MultiVecPrint(void **x, GCGE_INT n, GCGE_OPS *ops);
/* 
 *  Description:  测试PASE_MULTIGRID
 */
int
main ( int argc, char *argv[] )
{
    /* SlepcInitialize */
    SlepcInitialize(&argc,&argv,(char*)0,help);
    PetscErrorCode ierr;
    PetscMPIInt    rank;
    PetscViewer    viewer;
    MPI_Comm_rank(PETSC_COMM_WORLD, &rank);

    /* 得到细网格矩阵 */
    Mat      A, B;
    PetscInt n = 30, m = 30;
    GetPetscMat(&A, &B, n, m);

    //创建gcge_ops
    GCGE_OPS *gcge_ops;
    GCGE_OPS_CreateSLEPC(&gcge_ops);
    gcge_ops->MultiVecPrint = PETSC_Sub_MultiVecPrint;
    PASE_OPS *pase_ops;
    PASE_OPS_Create(&pase_ops, gcge_ops);

    //进行multigrid分层
    int nev = 5;
    int max_num_levels = 5;
    int mg_coarsest_level = 2;
    PASE_REAL convert_time = 0.0;
    PASE_REAL amg_time = 0.0;
    PASE_MULTIGRID multi_grid;
    int **size = (int**)malloc(5*sizeof(int*));
    int i = 0;
    int j = 0;
    for (i=0; i<5; i++) {
        size[i] = (int*)calloc(max_num_levels, sizeof(int));
        for (j=0; j<max_num_levels; j++) {
            size[i][j] = nev;
        }
    }
    int size_dtmp = nev*nev;
    int size_itmp = nev*nev;
    int error = PASE_MULTIGRID_Create(&multi_grid, max_num_levels, mg_coarsest_level, 
            size, size_dtmp, size_itmp,
            (void *)A, (void *)B, gcge_ops, &convert_time, &amg_time);

    BV *u,  *rhs,  *u_tmp,  *u_tmp_1,  *u_tmp_2;
    double *double_tmp; 
    int *int_tmp;
    u          = (BV*)multi_grid->sol;
    rhs        = (BV*)multi_grid->rhs;
    u_tmp      = (BV*)multi_grid->cg_p;
    u_tmp_1    = (BV*)multi_grid->cg_w;
    u_tmp_2    = (BV*)multi_grid->cg_res;
    double_tmp = (double*)multi_grid->cg_double_tmp;
    int_tmp    = (int*)multi_grid->cg_int_tmp;    

    gcge_ops->MultiVecSetRandomValue((void**)(u[0]), 0, nev, gcge_ops);
    printf ( "-----------------------------\n" );
    PETSCPrintBV(u[0], "exact u[0]");
    printf ( "-----------------------------\n" );


    int mv_s[2];
    int mv_e[2];

    mv_s[0] = 0;
    mv_e[0] = nev;
    mv_s[1] = 0;
    mv_e[1] = nev;

    //计算右端项
    gcge_ops->MatDotMultiVec(A, (void**)(u[0]), (void**)(rhs[0]), 
	  mv_s, mv_e, gcge_ops);
    //-----------------------------------------------

    gcge_ops->MultiVecSetRandomValue((void**)(u[0]), 0, nev, gcge_ops);
    PETSCPrintBV(u[0], "initial u[0]");
    printf ( "-----------------------------\n" );
    //计算初始残差
    mv_s[0] = 0;
    mv_e[0] = nev;
    mv_s[1] = 0;
    mv_e[1] = nev;

    gcge_ops->MatDotMultiVec(A, (void**)(u[0]), (void**)(u_tmp[0]), mv_s, mv_e, gcge_ops);
    mv_s[0] = 0;
    mv_e[0] = nev;
    mv_s[1] = 0;
    mv_e[1] = nev;
    gcge_ops->MultiVecAxpby(1.0, (void**)(rhs[0]), -1.0, (void**)(u_tmp[0]), 
	  mv_s, mv_e, gcge_ops);
    gcge_ops->MultiVecInnerProd((void**)(u_tmp[0]), (void**)(u_tmp[0]), 
	  double_tmp, "nonsym", mv_s, mv_e, nev, 0, gcge_ops);
    for(i=0; i<nev; i++) {
        GCGE_Printf("init residual[%d] = %e\n", i, double_tmp[i*nev+i]);
    }


    double tol = 1e-8;
    double rate = 1e-4;
    int nsmooth = 1000;
    int max_coarest_nsmooth = 10000;
    //PASE_BMG_TEST(multi_grid, 0, (void**)(rhs[0]), (void**)(u[0]), mv_s, mv_e, 

    int start[5];
    int end[5];
    start[0] = 0; end[0] = nev;
    start[1] = 0; end[1] = nev;
    start[2] = 0; end[2] = nev;
    start[3] = 0; end[3] = nev;
    start[4] = 0; end[4] = nev;
#if 1
    PASE_BMG(multi_grid, 0, (void**)(rhs[0]), (void**)(u[0]), start, end, 
     	  tol, rate, nsmooth, max_coarest_nsmooth);
#else
    GCGE_BCG(A, 0, 0.0, NULL, (void**)(rhs[0]), (void**)(u[0]), start, end, nsmooth, rate,
          gcge_ops, (void**)(u_tmp[0]), (void**)(u_tmp_1[0]), 
    	    (void**)(u_tmp_2[0]), double_tmp, int_tmp);
#endif
    PETSCPrintBV(u[0], "output u[0]");
    printf ( "-----------------------------\n" );

    mv_s[0] = 0;
    mv_e[0] = nev;
    mv_s[1] = 0;
    mv_e[1] = nev;
    //计算最后的残差
    gcge_ops->MatDotMultiVec(A, (void**)(u[0]), (void**)(u_tmp[0]), mv_s, mv_e, gcge_ops);
    mv_s[0] = 0;
    mv_e[0] = nev;
    mv_s[1] = 0;
    mv_e[1] = nev;
    gcge_ops->MultiVecAxpby(1.0, (void**)(rhs[0]), -1.0, (void**)(u_tmp[0]), 
	  mv_s, mv_e, gcge_ops);
    //PETSCPrintBV(u_tmp[0], "resi after");
    gcge_ops->MultiVecInnerProd((void**)(u_tmp[0]), (void**)(u_tmp[0]), 
	  double_tmp, "nonsym", mv_s, mv_e, nev, 0, gcge_ops);
    for(i=0; i<nev; i++) {
        GCGE_Printf("final residual[%d] = %e\n", i, double_tmp[i*nev+i]);
    }

    PASE_MULTIGRID_Destroy(&multi_grid, size);
    GCGE_OPS_Free(&gcge_ops);
    PASE_OPS_Free(&pase_ops); 
    ierr = MatDestroy(&A);
    ierr = MatDestroy(&B);
    for (i=0; i<5; i++){
        free(size[i]); size[i] = NULL;
    }
    free(size); size = NULL;

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
    for(i=0; i<3; i++)
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

void PASEGetCommandLineInfo(PASE_INT argc, char *argv[], PASE_INT *block_size, PASE_REAL *atol, PASE_INT *nsmooth)
{
  PASE_INT arg_index = 0;
  PASE_INT print_usage = 0;

  while (arg_index < argc)
  {
    if ( strcmp(argv[arg_index], "-block_size") == 0 )
    {
      arg_index++;
      *block_size = atoi(argv[arg_index++]);
    }
    else if ( strcmp(argv[arg_index], "-atol") == 0 )
    {
      arg_index++;
      *atol= pow(10, atoi(argv[arg_index++]));
    }
    else if ( strcmp(argv[arg_index], "-nsmooth") == 0 )
    {
      arg_index++;
      *nsmooth= atoi(argv[arg_index++]);
    }
    else if ( strcmp(argv[arg_index], "-help") == 0 )
    {
      print_usage = 1;
      break;
    }
    else
    {
      arg_index++;
    }
  }

  if(print_usage)
  {
    GCGE_Printf("\n");
    GCGE_Printf("Usage: %s [<options>]\n", argv[0]);
    GCGE_Printf("\n");
    GCGE_Printf("  -n <n>              : problem size in each direction (default: 33)\n");
    GCGE_Printf("  -block_size <n>      : eigenproblem block size (default: 3)\n");
    GCGE_Printf("  -max_levels <n>      : max levels of AMG (default: 5)\n");
    GCGE_Printf("\n");
    exit(-1);
  }
}

void PASE_BMG_TEST( PASE_MULTIGRID mg, 
               PASE_INT current_level, 
               void **rhs, void **sol, 
               PASE_INT *start, PASE_INT *end,
               PASE_REAL tol, PASE_REAL rate, 
               PASE_INT nsmooth, PASE_INT max_coarest_nsmooth)
{
    PASE_INT nlevel = mg->num_levels;
    //默认0层为最细层
    PASE_INT indicator = 1;
    // obtain the coarsest level
    PASE_INT coarest_level;
    if( indicator > 0 )
        coarest_level = nlevel-1;
    else
        coarest_level = 0;
    //设置最粗层上精确求解的精度
    PASE_REAL coarest_rate = 1e-8;
    void *A;
    PASE_INT mv_s[2];
    PASE_INT mv_e[2];
    void **residual = mg->cg_p[current_level];
    // obtain the 'enough' accurate solution on the coarest level
    //direct solving the linear equation
    if( current_level == coarest_level )
    {
        //最粗层？？？？？？？
        A = mg->A_array[coarest_level];
        GCGE_BCG(A, 0, 0.0, NULL, rhs, sol, start, end, 
                max_coarest_nsmooth, coarest_rate, mg->gcge_ops, 
                mg->cg_p[coarest_level], mg->cg_w[coarest_level], 
                mg->cg_res[coarest_level], 
                mg->cg_double_tmp, mg->cg_int_tmp);
    }
    else
    {   
        A = mg->A_array[current_level];
        GCGE_BCG(A, 0, 0.0, NULL, rhs, sol, start, end, 
                nsmooth, rate, mg->gcge_ops, 
                mg->cg_p[current_level], mg->cg_w[current_level], 
		mg->cg_res[current_level], 
                mg->cg_double_tmp, mg->cg_int_tmp);

        mv_s[0] = start[1];
        mv_e[0] = end[1];
        mv_s[1] = 0;
        mv_e[1] = end[1]-start[1];
        //计算residual = A*sol
        mg->gcge_ops->MatDotMultiVec(A, sol, residual, mv_s, mv_e, mg->gcge_ops);
        //计算residual = rhs-A*sol
        mv_s[0] = 0;
        mv_e[0] = end[1]-start[1];
        mv_s[1] = 0;
        mv_e[1] = end[1]-start[1];
        mg->gcge_ops->MultiVecAxpby(1.0, rhs, -1.0, residual, mv_s, mv_e, mg->gcge_ops);

        // 把 residual 投影到粗网格
        PASE_INT coarse_level = current_level + indicator;
        void **coarse_residual = mg->rhs[coarse_level];
        mv_s[0] = 0;
        mv_e[0] = end[1]-start[1];
        mv_s[1] = 0;
        mv_e[1] = end[1]-start[1];
        PASE_INT error = PASE_MULTIGRID_FromItoJ(mg, current_level, coarse_level, 
                mv_s, mv_e, residual, coarse_residual);
        /* TODO coarse_sol????? */

        //求粗网格解问题，利用递归
        void **coarse_sol = mg->sol[coarse_level];
        mv_s[0] = 0;
        mv_e[0] = end[1]-start[1];
        mv_s[1] = 0;
        mv_e[1] = end[1]-start[1];
	//先给coarse_sol赋初值0
	mg->gcge_ops->MultiVecAxpby(0.0, coarse_sol, 0.0, coarse_sol, 
	        mv_s, mv_e, mg->gcge_ops);
        //PASE_BMG(mg, coarse_level, coarse_residual, coarse_sol, 
        PASE_BMG_TEST(mg, coarse_level, coarse_residual, coarse_sol, 
                mv_s, mv_e, tol, rate, nsmooth, max_coarest_nsmooth);
        
        // 把粗网格上的解插值到细网格，再加到前光滑得到的近似解上
        // 可以用 residual 代替
        mv_s[0] = 0;
        mv_e[0] = end[1]-start[1];
        mv_s[1] = 0;
        mv_e[1] = end[1]-start[1];
        error = PASE_MULTIGRID_FromItoJ(mg, coarse_level, current_level, 
                mv_s, mv_e, coarse_sol, residual);
        //计算residual = rhs-A*sol
        mv_s[0] = 0;
        mv_e[0] = end[1]-start[1];
        mv_s[1] = start[1];
        mv_e[1] = end[1];
        mg->gcge_ops->MultiVecAxpby(1.0, residual, 1.0, sol, mv_s, mv_e, mg->gcge_ops);
        
	//后光滑
        GCGE_BCG(A, 0, 0.0, NULL, rhs, sol, start, end, 
                nsmooth, rate, mg->gcge_ops, 
                mg->cg_p[current_level], mg->cg_w[current_level], 
		mg->cg_res[current_level], 
                mg->cg_double_tmp, mg->cg_int_tmp);
    }//end for (if current_level)
}

void PETSC_Sub_MultiVecPrint(void **x, GCGE_INT n, GCGE_OPS *ops)
{
    PETSCPrintBV((BV)x, "gcge_ops->MultiVecPrint");
}
