/*
 * =====================================================================================
 *
 *       Filename:  test_convert_phg.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2019年01月22日 09时57分13秒
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
#include <time.h>

/* ERROR can not include pase_convert.h
 * PHG functions can not be complied with PHG functions
#include "pase_convert.h"
*/
#include "HYPRE.h"
#include "HYPRE_IJ_mv.h"
#include "_hypre_parcsr_mv.h"
#include "phg.h"

#if (PHG_VERSION_MAJOR <= 0 && PHG_VERSION_MINOR < 9)
# undef ELEMENT
typedef SIMPLEX ELEMENT;
#endif

static void
build_matrices(MAT *matA, MAT *matB, DOF *u_h)
{
    int N = u_h->type->nbas;	/* number of basis functions in an element */
    int i, j;
    GRID *g = u_h->g;
    ELEMENT *e;
    FLOAT A[N][N], B[N][N], vol;
    static FLOAT *B0 = NULL;
    int I[N];

    if (B0 == NULL && g->nroot > 0) {
	/* (\int \phi_j\cdot\phi_i)/vol is independent of element */
	FreeAtExit(B0);
	B0 = phgAlloc(N * N * sizeof(*B0));
	e = g->roots;
	vol = phgGeomGetVolume(g, e);
	for (i = 0; i < N; i++)
	    for (j = 0; j <= i; j++)
		B0[i * N + j] = B0[i + j * N] =
		    phgQuadBasDotBas(e, u_h, j, u_h, i, QUAD_DEFAULT) / vol;
    }

    assert(u_h->dim == 1);
    ForAllElements(g, e) {
	vol = phgGeomGetVolume(g, e);
	for (i = 0; i < N; i++) {
	    I[i] = phgMapE2L(matA->cmap, 0, e, i);
	    for (j = 0; j <= i; j++) {
		/* \int \grad\phi_j\cdot\grad\phi_i */
		A[j][i] = A[i][j] =
		    phgQuadGradBasDotGradBas(e, u_h, j, u_h, i, QUAD_DEFAULT);
		/* \int \phi_j\cdot\phi_i */
		B[j][i] = B[i][j] = B0[i * N + j] * vol;
	    }
	}

	/* loop on basis functions */
	for (i = 0; i < N; i++) {
	    if (phgDofDirichletBC(u_h, e, i, NULL, NULL, NULL, DOF_PROJ_NONE))
		continue;
	    phgMatAddEntries(matA, 1, I + i, N, I, A[i]); 
	    phgMatAddEntries(matB, 1, I + i, N, I, B[i]); 
	}
    }
}

static int
bc_map(int bctype)
{
    return DIRICHLET;	/* set Dirichlet BC on all boundaries */
}

/* PHG -> HYPRE */
int
main(int argc, char *argv[])
{
    static char *fn = "./data/cube4.dat";
    static int mem_max = 3000;
    size_t mem, mem_peak;
    int i, j, k, n, nit;
    int pre_refines = 1;
    GRID *g;
    DOF *u_h;
    MAP *map;
    MAT *A, *B;
    double wtime;

    phgOptionsPreset("-dof_type P1");

    phgOptionsRegisterFilename("-mesh_file", "Mesh filename", &fn);
    phgOptionsRegisterInt("-pre_refines", "Pre-refines", &pre_refines);
    phgOptionsRegisterInt("-mem_max", "Maximum memory (MB)", &mem_max);

    phgInit(&argc, &argv);

    if (DOF_DEFAULT->mass_lumping == NULL)
	phgPrintf("Order of FE bases: %d\n", DOF_DEFAULT->order);
    else
	phgPrintf("Order of FE bases: %d\n", DOF_DEFAULT->mass_lumping->order0);

    g = phgNewGrid(-1);
    phgImportSetBdryMapFunc(bc_map);
    /*phgSetPeriodicity(g, X_MASK | Y_MASK | Z_MASK);*/
    if (!phgImport(g, fn, FALSE))
	phgError(1, "can't read file \"%s\".\n", fn);
    /* pre-refinement */
    phgRefineAllElements(g, pre_refines);

    u_h = phgDofNew(g, DOF_DEFAULT, 1, "u_h", DofInterpolation);
#if 0
    /* All-Neumann BC */
    phgDofSetDirichletBoundaryMask(u_h, 0);
#else
    /* All-Dirichlet BC */
    phgDofSetDirichletBoundaryMask(u_h, BDRY_MASK);
#endif
    /* set random initial values for the eigenvectors */
    phgDofRandomize(u_h, i == 0 ? 123 : 0);

    phgPrintf("\n");
    if (phgBalanceGrid(g, 1.2, -1, NULL, 0.))
       phgPrintf("Repartition mesh\n");
    phgPrintf("%"dFMT" DOF, %"dFMT
	  " elements, %d submesh%s, load imbalance: %lg\n",
	  DofGetDataCountGlobal(u_h), g->nleaf_global,
	  g->nprocs, g->nprocs > 1 ? "es" : "", (double)g->lif);
    wtime = phgGetTime(NULL);
    map = phgMapCreate(u_h, NULL);
    {
       MAP *m = phgMapRemoveBoundaryEntries(map);
       A = phgMapCreateMat(m, m);
       B = phgMapCreateMat(m, m);
       phgMapDestroy(&m);
    }
    build_matrices(A, B, u_h);
    phgPrintf("Build matrices, matrix size: %"dFMT", wtime: %0.2lfs\n",
	  A->rmap->nglobal, phgGetTime(NULL) - wtime);
    wtime = phgGetTime(NULL);

#if 0
    PHG这样转PETSC有问题, 首先PHG会重定义一些PETSC定义的东西
    Mat *petsc_mat;
    PetscViewer viewer;
    MatrixConvertPHG2PETSC(petsc_mat, A);
    MatView(*petsc_mat, viewer);
    MatDestroy(petsc_mat);
#else
    HYPRE_IJMatrix hypre_ij_mat;

    MatrixConvertPHG2HYPRE(&hypre_ij_mat, A);

    int row_start, row_end, col_start, col_end, rank;
    MPI_Comm_rank(MPI_COMM_WORLD,  &rank);
    HYPRE_IJMatrixGetLocalRange(hypre_ij_mat,  &row_start,  &row_end,  &col_start,  &col_end);
    printf ( "hypre,  %d: row_start = %d,  row_end = %d\n",  rank,  row_start,  row_end );
    printf ( "hypre,  %d: col_start = %d,  col_end = %d\n",  rank,  col_start,  col_end );

    HYPRE_ParCSRMatrix hypre_parcsr_mat;
    HYPRE_IJMatrixGetObject(hypre_ij_mat, (void**) &hypre_parcsr_mat);

//    Mat petsc_mat_A;
//    MatrixConvertHYPRE2PETSC(&petsc_mat_A, hypre_parcsr_mat);
//    MatDestroy(&petsc_mat_A);

    HYPRE_IJMatrixDestroy(hypre_ij_mat);
#endif






    phgMatDestroy(&B);
    phgMatDestroy(&A);
    phgMapDestroy(&map);
    phgDofFree(&u_h);
    phgFreeGrid(&g);

    phgFinalize();
    return 0;
}
