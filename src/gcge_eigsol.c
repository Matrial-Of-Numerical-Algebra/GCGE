/*
 * =====================================================================================
 *
 *       Filename:  gcge_eigsol.c
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

#include "gcge_eigsol.h"

/**
 * @brief 计算 A * evec = eval * B * evec 
 *    
 *    TODO 考虑X正交化时，如果有相关性，重新选随机数,至少要报错（初始化失败）
 *
 *    Initialization: 
 *       对evec设定随机初值,把evec复制给X
 *       对X正交化,在X张成的子空间中计算RR问题
 *       子空间矩阵subspace_matrix, 子空间特征向量subspace_evec, 
 *       特征值workspace->eval
 *       检查收敛性，得到unlock,num_unlock
 *
 *       对未收敛的X计算W
 *       对W进行正交化
 *       在[X,W]张成的子空间中计算RR问题
 *       检查收敛性，得到unlock,num_unlock
 *       
 *    Loop: 
 *       对未收敛的X计算P
 *       对未收敛的X计算W
 *       对W进行正交化
 *       在[X,W]张成的子空间中计算RR问题
 *       检查特征值重数，调整X的长度
 *       检查收敛性，得到unlock,num_unlock
 *
 *    Finalization:
 *       把X复制给evec
 *       把workspace->eval复制给eval
 *
 * @param A
 * @param B
 * @param eval
 * @param evec
 * @param para
 * @param ops
 * @param workspace
 */
//求解特征值调用GCGE_Eigen函数
//A,B表示要求解特征值的矩阵GCGE_MATRIX，evec是要求解的特征向量GCGE_Vec
void GCGE_EigenSolver(void *A, void *B, GCGE_DOUBLE *eval, void **evec, 
        GCGE_PARA *para, GCGE_OPS *ops, GCGE_WORKSPACE *workspace)
{
    //统计GCGE求解时间
    GCGE_DOUBLE ttotal1 = GCGE_GetTime();
    /* TODO */
    //在外面分配空间，setup
    //GCGE_WORKSPACE_Create(solver);


    GCGE_INT    nev = para->nev;
    GCGE_INT    max_dim_x = workspace->max_dim_x;
    GCGE_INT    ev_max_it = para->ev_max_it;
    GCGE_DOUBLE *subspace_matrix = workspace->subspace_matrix;
    GCGE_DOUBLE *subspace_evec = workspace->subspace_evec;
    void        **V = workspace->V;
    void        **RitzVec = workspace->RitzVec;
    void        *Orth_mat; 
    void        *RayleighRitz_mat;
    
    GCGE_INT    i = 0;
    GCGE_INT    mv_s[2] = { 0, 0 };
    GCGE_INT    mv_e[2]   = { 0, 0 };

    //如果用户不给初值，这里给随机初值，用户需要提供给向量设随机值的函数
    if(para->given_init_evec == 0)
    {
        ops->MultiVecSetRandomValue(evec, nev, ops);
    }

    //把用户提供的evec初值copy给V
    mv_s[0] = 0;
    mv_e[0] = nev;
    mv_s[1] = 0;
    mv_e[1] = nev;
    ops->MultiVecAxpby(1.0, evec, 0.0, V, mv_s, mv_e, ops);

    //如果使用B内积，那么用矩阵B进行正交化，用矩阵A计算子空间矩阵
    //如果使用A内积，那么用矩阵A进行正交化，用矩阵B计算子空间矩阵
    if(strcmp(para->orth_type, "B"))
    {
        Orth_mat = A;
        RayleighRitz_mat = B;
    }
    else
    {
        Orth_mat = B;
        RayleighRitz_mat = A;
    }

    //对初始近似特征向量(V的第0至第dim_x列)做B正交化,
    //如果正交化中出现0向量，dim_x将被更新为非零向量的个数
    workspace->dim_x = nev;
    GCGE_Orthogonal(V, 0, &(workspace->dim_x), Orth_mat, ops, para->orth_para, 
            workspace->V_tmp, workspace->subspace_dtmp);
    if(workspace->dim_x < nev)
    {
        printf("The initial eigenvectors is linearly dependent!\n");
    }
    //dim_xpw表示V=[X,P,W]的总向量个数，此时V中只有X
    workspace->dim_xpw = workspace->dim_x;
    workspace->last_dim_x = workspace->dim_x;
    workspace->dim_xp = 0;
    //计算得到子空间矩阵subspace_matrix=V^TAV
    GCGE_ComputeSubspaceMatrix(RayleighRitz_mat, V, ops, para, workspace);
    //计算子空间矩阵的特征对
    GCGE_ComputeSubspaceEigenpairs(subspace_matrix, workspace->eval, subspace_evec, 
            ops, para, workspace);
    for(i=0; i<10; i++)
    {
        printf("num_iter: %d, num_unlock: %d, eval[%d]: %e;\n", 
                para->num_iter, para->num_unlock, i, workspace->eval[i]);
    }
    //用子空间特征向量与基向量 V 计算Ritz向量
    GCGE_ComputeX(V, subspace_evec, RitzVec, ops, workspace);
    //检查收敛性
    GCGE_CheckConvergence(A, B, workspace->eval, RitzVec, ops, para, workspace);
    //Ritz向量放到V中作为下次迭代中基向量的一部分,即为[X,P,W]中的X
    mv_s[0] = 0;
    mv_s[1] = 0;
    mv_e[0] = workspace->dim_x;
    mv_e[1] = workspace->dim_x;
    ops->MultiVecSwap(V, RitzVec, mv_s, mv_e, ops);
    //dim_xp为X,P向量个数，也是V中W向量存放的初始位置
    workspace->dim_xp = workspace->dim_x;
    //用未收敛的特征向量作为X求解线性方程组 A W = \lambda B X
    GCGE_ComputeW(A, B, V, workspace->eval, ops, para, workspace);
    workspace->last_dim_xpw = workspace->dim_xpw;
    workspace->dim_xpw = workspace->dim_xp + workspace->unconv_bs;
    //对基向量组V的第dim_x列到dim_xpw列进行正交化
    GCGE_Orthogonal(V, workspace->dim_x, &(workspace->dim_xpw), Orth_mat, ops, para->orth_para,
            workspace->V_tmp, workspace->subspace_dtmp);
    workspace->dim_xp = 0;
    //计算子空间矩阵subspace_matrix = V^T*A*V
    GCGE_ComputeSubspaceMatrix(RayleighRitz_mat, V, ops, para, workspace);
    //计算子空间矩阵的特征对
    GCGE_ComputeSubspaceEigenpairs(subspace_matrix, workspace->eval, subspace_evec, 
            ops, para, workspace);
    //用子空间特征向量与基向量 V 计算Ritz向量
    GCGE_ComputeX(V, subspace_evec, RitzVec, ops, workspace);
    //检查收敛性
    GCGE_CheckConvergence(A, B, workspace->eval, RitzVec, ops, para, workspace);

    for(i=0; i<10; i++)
    {
        printf("num_iter: %d, num_unlock: %d, eval[%d]: %e;\n", 
                para->num_iter, para->num_unlock, i, workspace->eval[i]);
    }

    //--------------------开始循环--------------------------------------------
    while((para->num_unlock > 0)&&(para->num_iter < ev_max_it))
    {
        //计算P=X_j-X_{j-1},这次迭代用的X减去上一次迭代用的X
        GCGE_ComputeP(subspace_evec, V, ops, para, workspace);
        //Ritz向量放到V中作为下次迭代中基向量的一部分
        mv_s[0] = 0;
        mv_s[1] = 0;
        mv_e[0] = workspace->dim_x;
        mv_e[1] = workspace->dim_x;
        ops->MultiVecSwap(V, RitzVec, mv_s, mv_e, ops);
        //用未收敛的特征向量作为X求解线性方程组 A W = \lambda B X
        GCGE_ComputeW(A, B, V, workspace->eval, ops, para, workspace);
        workspace->last_dim_xpw = workspace->dim_xpw;
        workspace->dim_xpw = workspace->dim_xp + workspace->unconv_bs;
        //对W与前dim_xp个向量进行正交化
        //对基向量组V的第dim_xp列到dim_xpw列，即是W部分进行正交化
        GCGE_Orthogonal(V, workspace->dim_xp, &(workspace->dim_xpw), Orth_mat, ops, para->orth_para,
            workspace->V_tmp, workspace->subspace_dtmp);
        //计算子空间矩阵subspace_matrix = V^T*A*V
        GCGE_ComputeSubspaceMatrix(RayleighRitz_mat, V, ops, para, workspace);
        //计算子空间矩阵的特征对
        GCGE_ComputeSubspaceEigenpairs(subspace_matrix, workspace->eval, subspace_evec, 
            ops, para, workspace);
        workspace->last_dim_x = workspace->dim_x;
        //检查特征值重数,确定X空间的维数
        GCGE_CheckEvalMultiplicity(nev+2, max_dim_x, &(workspace->dim_x), workspace->eval);
        //用子空间特征向量与基向量 V 计算Ritz向量
        GCGE_ComputeX(V, subspace_evec, RitzVec, ops, workspace);
        //检查收敛性
        GCGE_CheckConvergence(A, B, workspace->eval, RitzVec, ops, para, workspace);

        for(i=0; i<10; i++)
        {
            printf("num_iter: %d, num_unlock: %d, eval[%d]: %e, res: %e;\n", 
                    para->num_iter, para->num_unlock, i, workspace->eval[i], para->res[i]);
        }

    }


    //把计算得到的近似特征对拷贝给eval,evec输出
    memcpy(eval, workspace->eval, nev*sizeof(GCGE_DOUBLE));

    mv_s[0] = 0;
    mv_e[0]   = nev;
    mv_s[1] = 0;
    mv_e[1]   = nev;
    ops->MultiVecAxpby(1.0, RitzVec, 0.0, evec, mv_s, mv_e, ops);

#if 0
    GCGE_DOUBLE ttotal2 = GCGE_GetTime();
    para->stat_para->total_time = ttotal2-ttotal1;
#endif
    //输出总时间信息，以及最终求得的特征值与残差信息
    /*
    if(para->print_result == 1)
    {
#if USE_MPI
        GCGE_PrintFinalInfoParallel(solver);
#else
        GCGE_PrintFinalInfo(solver);
#endif
    }
    */

    //释放GCGE工作空间
    //GCGE_WORKSPACE_Free(solver);
}

//计算残差，检查收敛性，并获取未收敛的特征对编号及个数
//A,B是用来计算残差的矩阵(GCGE_MATRIX), evec是特征向量(GCGE_Vec)
void GCGE_CheckConvergence(void *A, void *B, GCGE_DOUBLE *eval, void **evec, 
        GCGE_OPS *ops, GCGE_PARA *para, GCGE_WORKSPACE *workspace)
{
    /* TODO */
#if 0
    GCGE_DOUBLE    t1 = GCGE_GetTime();
#endif
    GCGE_INT    i, j, k, *unlock = workspace->unlock,
                unconv_bs = 0, max_conv_idx = 0,
                max_ind = 0, min_ind = 0,
                num_unlock = para->num_unlock;
    GCGE_DOUBLE res_norm, evec_norm, residual, 
                max_res = 0.0, min_res = 0.0, sum_res = 0.0,
                ev_tol = para->ev_tol,
                *res = para->res;
    char        *conv_type = para->conv_type;
    void        *tmp1, *tmp2, *ev;
    ops->GetVecFromMultiVec(workspace->V_tmp, 0, &tmp1);
    ops->GetVecFromMultiVec(workspace->V_tmp, 1, &tmp2);
    for( j=0; j<num_unlock; j++ )
    {
        i = unlock[j];
        ops->GetVecFromMultiVec(evec, i, &ev);
        //计算残量
        ops->MatDotVec(A, ev, tmp1);
        if(B == NULL)
        {
            ops->VecAxpby(-eval[i], ev, 1.0, tmp1);
        }
        else
        {
            ops->MatDotVec(B, ev, tmp2);
            ops->VecAxpby(-eval[i], tmp2, 1.0, tmp1);
        }
        res_norm  = GCGE_VecNorm(tmp1, ops);
        evec_norm = GCGE_VecNorm(ev, ops);
        //计算相对/绝对残差
        if(strcmp(conv_type, "R"))
        {
            residual  = res_norm/evec_norm;
        }
        else
        {
            residual  = res_norm/evec_norm/(1.0>fabs(eval[i])?1.0:fabs(eval[i]));
        }
        res[i] = residual;
        //统计最大、最小、总残差
        sum_res += residual;
        if(i == 0)
        {
            max_res = residual;
            min_res = residual;
        }
        else
        {
            if(residual > max_res)
            {
                max_res = residual;
                max_ind = i;
            }
            if(residual < min_res)
            {
                min_res = residual;
                min_ind = i;
            }
        }
        //统计未收敛的特征对编号unlock及数量
        if(residual > ev_tol)
        {
            unlock[unconv_bs] = i;
            unconv_bs += 1;
            //只比收敛的最后一个多算10个
            if(j > max_conv_idx+8)
            {
                for(k=j+1; k<num_unlock; k++)
                {
                    unlock[unconv_bs] = unlock[k];
                    unconv_bs += 1;
                }
                j = num_unlock+1;
            }
        }
        else
        {
            max_conv_idx = j;
        }

        ops->RestoreVecForMultiVec(evec, i, &ev);
    }
    ops->RestoreVecForMultiVec(workspace->V_tmp, 0, &tmp1);
    ops->RestoreVecForMultiVec(workspace->V_tmp, 1, &tmp2);

    para->num_unlock = unconv_bs;
    workspace->unconv_bs = (unconv_bs < para->block_size) ? unconv_bs : para->block_size;

    para->max_res = max_res;
    para->max_ind = max_ind;
    para->min_res = min_res;
    para->min_ind = min_ind;
    para->sum_res = sum_res;

    para->num_iter += 1;
    //打印收敛信息
    /*
    if(para->print_result == 1)
    {
#if USE_MPI
        GCGE_PrintConvInfoParallel(solver);
#else
        GCGE_PrintConvInfo(solver);
#endif
    }
    */

#if 0 
    GCGE_DOUBLE t2 = GCGE_GetTime();
    para->stat_para->ite_time = t2 - para->stat_para->ite_time;
    para->stat_para->PartTimeTotal->Conv_Time += t2-t1;
    para->stat_para->PartTimeInOneIte->Conv_Time = t2-t1;
#endif
}

//检查特征值重数，更新dim_x
void GCGE_CheckEvalMultiplicity(GCGE_INT start, GCGE_INT end, GCGE_INT *dim_x, GCGE_DOUBLE *eval)
{
    GCGE_INT tmp, i;
    tmp = start;
    //dsygv求出的特征值已经排序是ascending,从小到大
    //检查特征值的数值确定下次要进行计算的特征值个数
    for( i=start; i<end; i++ )
    {
        if((fabs(fabs(eval[tmp]/eval[tmp-1])-1))<0.2)
            tmp += 1;
        else
            break;
    }
    *dim_x = tmp;
}

