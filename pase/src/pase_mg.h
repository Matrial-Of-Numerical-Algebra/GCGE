#ifndef _pase_mg_h_
#define _pase_mg_h_

#include "gcge.h"
#include "gcge_app_slepc.h"

#include "pase_config.h"
#include "pase_convert.h"
#include "_hypre_parcsr_ls.h"
#include <petscmat.h>
#include <petscvec.h>

#define pase_MultiGridDataAArray(data)  ((data)->A_array)
#define pase_MultiGridDataBArray(data)  ((data)->B_array)
#define pase_MultiGridDataPArray(data)  ((data)->P_array)
#define pase_MultiGridDataQArray(data)  ((data)->Q_array)
#define pase_MultiGridDataUArray(data)  ((data)->U_array)
#define pase_MultiGridDataFArray(data)  ((data)->F_array)

#define pase_MultiGridDataNumLevels(data) ((data)->num_levels)
      
typedef struct pase_MultiGrid_struct 
{
   GCGE_INT num_levels;
   GCGE_INT coarsest_level;
   void      **A_array;
   void      **B_array;
   void      **P_array;
   /* P0P1P2  P1P2  P2 */
   //void     **Q_array;
   void     ***sol;
   void     ***rhs;
   void     ***cg_p;
   void     ***cg_w;
   void     ***cg_res;

   GCGE_DOUBLE  *cg_double_tmp;
   GCGE_INT   *cg_int_tmp;

   GCGE_OPS   *gcge_ops;
   
} pase_MultiGrid;
typedef struct pase_MultiGrid_struct *PASE_MULTIGRID;

GCGE_INT 
PASE_MULTIGRID_Create(PASE_MULTIGRID* multi_grid, 
        GCGE_INT max_levels, GCGE_INT mg_coarsest_level, 
	PASE_INT **size,  PASE_INT size_dtmp,  PASE_INT size_itmp, 
        void *A, void *B, GCGE_OPS *gcge_ops,
	GCGE_DOUBLE *convert_time, GCGE_DOUBLE *amg_time);

GCGE_INT PASE_MULTIGRID_Destroy(PASE_MULTIGRID* multi_grid, PASE_INT **size);


/**
 * TODO
 * @brief 将多向量从第level_i层 prolong/restrict 到level_j层
 *
 * @param multigrid 多重网格结构
 * @param level_i   起始层
 * @param level_j   目标层
 * @param mv_s      多向量pvx_i与pvx_j的起始位置
 * @param mv_e      多向量pvx_i与pvx_j的终止位置
 * @param pvx_i     起始多向量
 * @param pvx_j     目标多向量
 *
 * @return 
 */
GCGE_INT PASE_MULTIGRID_FromItoJ(PASE_MULTIGRID multi_grid, 
        GCGE_INT level_i, GCGE_INT level_j, 
        GCGE_INT *mv_s, GCGE_INT *mv_e, 
        void **pvx_i, void** pvx_j);

#endif
