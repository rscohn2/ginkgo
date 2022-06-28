/*******************************<GINKGO LICENSE>******************************
Copyright (c) 2017-2022, the Ginkgo authors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************<GINKGO LICENSE>*******************************/

#include "core/factorization/cholesky_kernels.hpp"


#include <algorithm>
#include <memory>


#include <ginkgo/core/matrix/csr.hpp>


#include "core/components/fill_array_kernels.hpp"
#include "core/factorization/elimination_forest.hpp"
#include "hip/base/hipsparse_bindings.hip.hpp"
#include "hip/base/math.hip.hpp"
#include "hip/components/cooperative_groups.hip.hpp"
#include "hip/components/intrinsics.hip.hpp"
#include "hip/components/reduction.hip.hpp"
#include "hip/components/thread_ids.hip.hpp"


namespace gko {
namespace kernels {
namespace hip {
/**
 * @brief The Cholesky namespace.
 *
 * @ingroup factor
 */
namespace cholesky {


constexpr int default_block_size = 512;


#include "common/cuda_hip/factorization/cholesky_kernels.hpp.inc"


template <typename ValueType, typename IndexType>
void cholesky_symbolic_count(
    std::shared_ptr<const DefaultExecutor> exec,
    const matrix::Csr<ValueType, IndexType>* mtx,
    const factorization::elimination_forest<IndexType>& forest,
    IndexType* row_nnz, array<IndexType>& tmp_storage)
{
    const auto num_rows = static_cast<IndexType>(mtx->get_size()[0]);
    if (num_rows == 0) {
        return;
    }
    const auto mtx_nnz = static_cast<IndexType>(mtx->get_num_stored_elements());
    tmp_storage.resize_and_reset(mtx_nnz + num_rows);
    const auto postorder_cols = tmp_storage.get_data();
    const auto lower_ends = postorder_cols + mtx_nnz;
    const auto row_ptrs = mtx->get_const_row_ptrs();
    const auto cols = mtx->get_const_col_idxs();
    const auto inv_postorder = forest.inv_postorder.get_const_data();
    const auto postorder_parent = forest.postorder_parents.get_const_data();
    // transform col indices to postorder indices
    {
        const auto num_blocks = ceildiv(num_rows, default_block_size);
        build_postorder_cols<<<num_blocks, default_block_size>>>(
            num_rows, cols, row_ptrs, inv_postorder, postorder_cols,
            lower_ends);
    }
    // sort postorder_cols inside rows
    {
        const auto handle = exec->get_hipsparse_handle();
        auto descr = hipsparse::create_mat_descr();
        array<IndexType> permutation_array(exec, mtx_nnz);
        auto permutation = permutation_array.get_data();
        components::fill_seq_array(exec, permutation, mtx_nnz);
        size_type buffer_size{};
        hipsparse::csrsort_buffer_size(handle, num_rows, num_rows, mtx_nnz,
                                       row_ptrs, postorder_cols, buffer_size);
        array<char> buffer_array{exec, buffer_size};
        auto buffer = buffer_array.get_data();
        hipsparse::csrsort(handle, num_rows, num_rows, mtx_nnz, descr, row_ptrs,
                           postorder_cols, permutation, buffer);
        hipsparse::destroy(descr);
    }
    // count nonzeros per row of L
    {
        const auto num_blocks =
            ceildiv(num_rows, default_block_size / config::warp_size);
        cholesky_symbolic_count_kernel<config::warp_size>
            <<<num_blocks, default_block_size>>>(num_rows, row_ptrs, lower_ends,
                                                 postorder_cols,
                                                 postorder_parent, row_nnz);
    }
}

GKO_INSTANTIATE_FOR_EACH_VALUE_AND_INDEX_TYPE(
    GKO_DECLARE_CHOLESKY_SYMBOLIC_COUNT);


template <typename ValueType, typename IndexType>
void cholesky_symbolic_factorize(
    std::shared_ptr<const DefaultExecutor> exec,
    const matrix::Csr<ValueType, IndexType>* mtx,
    const factorization::elimination_forest<IndexType>& forest,
    matrix::Csr<ValueType, IndexType>* l_factor,
    const array<IndexType>& tmp_storage)
{
    const auto num_rows = static_cast<IndexType>(mtx->get_size()[0]);
    if (num_rows == 0) {
        return;
    }
    const auto mtx_nnz = static_cast<IndexType>(mtx->get_num_stored_elements());
    const auto postorder_cols = tmp_storage.get_const_data();
    const auto lower_ends = postorder_cols + mtx_nnz;
    const auto row_ptrs = mtx->get_const_row_ptrs();
    const auto postorder = forest.postorder.get_const_data();
    const auto inv_postorder = forest.inv_postorder.get_const_data();
    const auto postorder_parent = forest.postorder_parents.get_const_data();
    const auto out_row_ptrs = l_factor->get_const_row_ptrs();
    const auto out_cols = l_factor->get_col_idxs();
    const auto num_blocks =
        ceildiv(num_rows, default_block_size / config::warp_size);
    cholesky_symbolic_factorize_kernel<config::warp_size>
        <<<num_blocks, default_block_size>>>(
            num_rows, row_ptrs, lower_ends, postorder_cols, postorder,
            postorder_parent, out_row_ptrs, out_cols);
}

GKO_INSTANTIATE_FOR_EACH_VALUE_AND_INDEX_TYPE(
    GKO_DECLARE_CHOLESKY_SYMBOLIC_FACTORIZE);


}  // namespace cholesky
}  // namespace hip
}  // namespace kernels
}  // namespace gko