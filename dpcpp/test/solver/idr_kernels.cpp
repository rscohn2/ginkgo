/*******************************<GINKGO LICENSE>******************************
Copyright (c) 2017-2021, the Ginkgo authors
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

#include <ginkgo/core/solver/idr.hpp>


#include <random>


#include <gtest/gtest.h>


#include <ginkgo/core/base/array.hpp>
#include <ginkgo/core/base/exception.hpp>
#include <ginkgo/core/base/executor.hpp>
#include <ginkgo/core/matrix/dense.hpp>
#include <ginkgo/core/stop/combined.hpp>
#include <ginkgo/core/stop/iteration.hpp>
#include <ginkgo/core/stop/residual_norm.hpp>


#include "core/solver/idr_kernels.hpp"
#include "core/test/utils.hpp"


namespace {


// use another alias to avoid conflict name in the Idr
template <typename Precision, typename OutputType = Precision>
using rr = typename gko::test::reduction_factor<Precision, OutputType>;

class Idr : public ::testing::Test {
protected:
#if GINKGO_DPCPP_SINGLE_MODE
    using value_type = float;
#else
    using value_type = double;
#endif
    using Mtx = gko::matrix::Dense<value_type>;
    using Solver = gko::solver::Idr<value_type>;

    Idr() : rand_engine(30) {}

    void SetUp()
    {
        ref = gko::ReferenceExecutor::create();
        dpcpp = gko::DpcppExecutor::create(0, ref);

        dpcpp_idr_factory =
            Solver::build()
                .with_deterministic(true)
                .with_criteria(
                    gko::stop::Iteration::build().with_max_iters(1u).on(dpcpp))
                .on(dpcpp);

        ref_idr_factory =
            Solver::build()
                .with_deterministic(true)
                .with_criteria(
                    gko::stop::Iteration::build().with_max_iters(1u).on(ref))
                .on(ref);
    }

    void TearDown()
    {
        if (dpcpp != nullptr) {
            ASSERT_NO_THROW(dpcpp->synchronize());
        }
    }

    std::unique_ptr<Mtx> gen_mtx(int num_rows, int num_cols)
    {
        return gko::test::generate_random_matrix<Mtx>(
            num_rows, num_cols,
            std::uniform_int_distribution<>(num_cols, num_cols),
            std::normal_distribution<gko::remove_complex<value_type>>(0.0, 1.0),
            rand_engine, ref);
    }

    void initialize_data(int size = 597, int input_nrhs = 17)
    {
        nrhs = input_nrhs;
        int s = 4;
        mtx = gen_mtx(size, size);
        x = gen_mtx(size, nrhs);
        b = gen_mtx(size, nrhs);
        r = gen_mtx(size, nrhs);
        m = gen_mtx(s, nrhs * s);
        f = gen_mtx(s, nrhs);
        g = gen_mtx(size, nrhs * s);
        u = gen_mtx(size, nrhs * s);
        c = gen_mtx(s, nrhs);
        v = gen_mtx(size, nrhs);
        p = gen_mtx(s, size);
        alpha = gen_mtx(1, nrhs);
        omega = gen_mtx(1, nrhs);
        tht = gen_mtx(1, nrhs);
        residual_norm = gen_mtx(1, nrhs);
        stop_status =
            std::make_unique<gko::Array<gko::stopping_status>>(ref, nrhs);
        for (size_t i = 0; i < nrhs; ++i) {
            stop_status->get_data()[i].reset();
        }

        d_mtx = gko::clone(dpcpp, mtx);
        d_x = gko::clone(dpcpp, x);
        d_b = gko::clone(dpcpp, b);
        d_r = gko::clone(dpcpp, r);
        d_m = gko::clone(dpcpp, m);
        d_f = gko::clone(dpcpp, f);
        d_g = gko::clone(dpcpp, g);
        d_u = gko::clone(dpcpp, u);
        d_c = gko::clone(dpcpp, c);
        d_v = gko::clone(dpcpp, v);
        d_p = gko::clone(dpcpp, p);
        d_alpha = gko::clone(dpcpp, alpha);
        d_omega = gko::clone(dpcpp, omega);
        d_tht = gko::clone(dpcpp, tht);
        d_residual_norm = gko::clone(dpcpp, residual_norm);
        d_stop_status = std::make_unique<gko::Array<gko::stopping_status>>(
            dpcpp, *stop_status);
    }

    std::shared_ptr<gko::ReferenceExecutor> ref;
    std::shared_ptr<const gko::DpcppExecutor> dpcpp;

    std::ranlux48 rand_engine;

    std::shared_ptr<Mtx> mtx;
    std::shared_ptr<Mtx> d_mtx;
    std::unique_ptr<Solver::Factory> dpcpp_idr_factory;
    std::unique_ptr<Solver::Factory> ref_idr_factory;

    gko::size_type nrhs;

    std::unique_ptr<Mtx> x;
    std::unique_ptr<Mtx> b;
    std::unique_ptr<Mtx> r;
    std::unique_ptr<Mtx> m;
    std::unique_ptr<Mtx> f;
    std::unique_ptr<Mtx> g;
    std::unique_ptr<Mtx> u;
    std::unique_ptr<Mtx> c;
    std::unique_ptr<Mtx> v;
    std::unique_ptr<Mtx> p;
    std::unique_ptr<Mtx> alpha;
    std::unique_ptr<Mtx> omega;
    std::unique_ptr<Mtx> tht;
    std::unique_ptr<Mtx> residual_norm;
    std::unique_ptr<gko::Array<gko::stopping_status>> stop_status;

    std::unique_ptr<Mtx> d_x;
    std::unique_ptr<Mtx> d_b;
    std::unique_ptr<Mtx> d_r;
    std::unique_ptr<Mtx> d_m;
    std::unique_ptr<Mtx> d_f;
    std::unique_ptr<Mtx> d_g;
    std::unique_ptr<Mtx> d_u;
    std::unique_ptr<Mtx> d_c;
    std::unique_ptr<Mtx> d_v;
    std::unique_ptr<Mtx> d_p;
    std::unique_ptr<Mtx> d_alpha;
    std::unique_ptr<Mtx> d_omega;
    std::unique_ptr<Mtx> d_tht;
    std::unique_ptr<Mtx> d_residual_norm;
    std::unique_ptr<gko::Array<gko::stopping_status>> d_stop_status;
};


TEST_F(Idr, IdrInitializeIsEquivalentToRef)
{
    initialize_data();

    gko::kernels::reference::idr::initialize(ref, nrhs, m.get(), p.get(), true,
                                             stop_status.get());
    gko::kernels::dpcpp::idr::initialize(dpcpp, nrhs, d_m.get(), d_p.get(),
                                         true, d_stop_status.get());

    GKO_ASSERT_MTX_NEAR(m, d_m, rr<value_type>::value);
    GKO_ASSERT_MTX_NEAR(p, d_p, rr<value_type>::value);
}


TEST_F(Idr, IdrStep1IsEquivalentToRef)
{
    initialize_data();

    gko::size_type k = 2;
    gko::kernels::reference::idr::step_1(ref, nrhs, k, m.get(), f.get(),
                                         r.get(), g.get(), c.get(), v.get(),
                                         stop_status.get());
    gko::kernels::dpcpp::idr::step_1(dpcpp, nrhs, k, d_m.get(), d_f.get(),
                                     d_r.get(), d_g.get(), d_c.get(), d_v.get(),
                                     d_stop_status.get());

    GKO_ASSERT_MTX_NEAR(c, d_c, rr<value_type>::value);
    GKO_ASSERT_MTX_NEAR(v, d_v, rr<value_type>::value);
}


TEST_F(Idr, IdrStep2IsEquivalentToRef)
{
    initialize_data();

    gko::size_type k = 2;
    gko::kernels::reference::idr::step_2(ref, nrhs, k, omega.get(), v.get(),
                                         c.get(), u.get(), stop_status.get());
    gko::kernels::dpcpp::idr::step_2(dpcpp, nrhs, k, d_omega.get(), d_v.get(),
                                     d_c.get(), d_u.get(), d_stop_status.get());

    GKO_ASSERT_MTX_NEAR(u, d_u, rr<value_type>::value);
}


TEST_F(Idr, IdrStep3IsEquivalentToRef)
{
    initialize_data();

    gko::size_type k = 2;
    gko::kernels::reference::idr::step_3(
        ref, nrhs, k, p.get(), g.get(), v.get(), u.get(), m.get(), f.get(),
        alpha.get(), r.get(), x.get(), stop_status.get());
    gko::kernels::dpcpp::idr::step_3(
        dpcpp, nrhs, k, d_p.get(), d_g.get(), d_v.get(), d_u.get(), d_m.get(),
        d_f.get(), d_alpha.get(), d_r.get(), d_x.get(), d_stop_status.get());

    GKO_ASSERT_MTX_NEAR(g, d_g, 2 * rr<value_type>::value);
    GKO_ASSERT_MTX_NEAR(v, d_v, 2 * rr<value_type>::value);
    GKO_ASSERT_MTX_NEAR(u, d_u, 2 * rr<value_type>::value);
    GKO_ASSERT_MTX_NEAR(m, d_m, 2 * rr<value_type>::value);
    GKO_ASSERT_MTX_NEAR(f, d_f, 13 * rr<value_type>::value);
    GKO_ASSERT_MTX_NEAR(r, d_r, 2 * rr<value_type>::value);
    GKO_ASSERT_MTX_NEAR(x, d_x, 2 * rr<value_type>::value);
}


TEST_F(Idr, IdrComputeOmegaIsEquivalentToRef)
{
    initialize_data();

    value_type kappa = 0.7;
    gko::kernels::reference::idr::compute_omega(ref, nrhs, kappa, tht.get(),
                                                residual_norm.get(),
                                                omega.get(), stop_status.get());
    gko::kernels::dpcpp::idr::compute_omega(dpcpp, nrhs, kappa, d_tht.get(),
                                            d_residual_norm.get(),
                                            d_omega.get(), d_stop_status.get());

    GKO_ASSERT_MTX_NEAR(omega, d_omega, rr<value_type>::value);
}


TEST_F(Idr, IdrIterationOneRHSIsEquivalentToRef)
{
    if (dpcpp->get_queue()->get_device().is_gpu()) {
        GTEST_SKIP() << "skip the test because oneMKL GEMM on gpu may give NaN "
                        "(under investigation)";
    }
    initialize_data(123, 1);
    auto ref_solver = ref_idr_factory->generate(mtx);
    auto dpcpp_solver = dpcpp_idr_factory->generate(d_mtx);

    ref_solver->apply(b.get(), x.get());
    dpcpp_solver->apply(d_b.get(), d_x.get());

    GKO_ASSERT_MTX_NEAR(d_b, b, rr<value_type>::value * 10);
    GKO_ASSERT_MTX_NEAR(d_x, x, rr<value_type>::value * 10);
}


TEST_F(Idr, IdrIterationWithComplexSubspaceOneRHSIsEquivalentToRef)
{
    initialize_data(123, 1);
    dpcpp_idr_factory =
        Solver::build()
            .with_deterministic(true)
            .with_complex_subspace(true)
            .with_criteria(
                gko::stop::Iteration::build().with_max_iters(1u).on(dpcpp))
            .on(dpcpp);
    ref_idr_factory =
        Solver::build()
            .with_deterministic(true)
            .with_complex_subspace(true)
            .with_criteria(
                gko::stop::Iteration::build().with_max_iters(1u).on(ref))
            .on(ref);
    auto ref_solver = ref_idr_factory->generate(mtx);
    auto dpcpp_solver = dpcpp_idr_factory->generate(d_mtx);

    ref_solver->apply(b.get(), x.get());
    dpcpp_solver->apply(d_b.get(), d_x.get());

    GKO_ASSERT_MTX_NEAR(d_b, b, rr<value_type>::value * 100);
    GKO_ASSERT_MTX_NEAR(d_x, x, rr<value_type>::value * 100);
}


TEST_F(Idr, IdrIterationMultipleRHSIsEquivalentToRef)
{
    initialize_data(123, 16);
    auto dpcpp_solver = dpcpp_idr_factory->generate(d_mtx);
    auto ref_solver = ref_idr_factory->generate(mtx);

    ref_solver->apply(b.get(), x.get());
    dpcpp_solver->apply(d_b.get(), d_x.get());

    GKO_ASSERT_MTX_NEAR(d_b, b, rr<value_type>::value * 500);
    GKO_ASSERT_MTX_NEAR(d_x, x, rr<value_type>::value * 500);
}


TEST_F(Idr, IdrIterationWithComplexSubspaceMultipleRHSIsEquivalentToRef)
{
    initialize_data(123, 6);
    dpcpp_idr_factory =
        Solver::build()
            .with_deterministic(true)
            .with_complex_subspace(true)
            .with_criteria(
                gko::stop::Iteration::build().with_max_iters(1u).on(dpcpp))
            .on(dpcpp);
    ref_idr_factory =
        Solver::build()
            .with_deterministic(true)
            .with_complex_subspace(true)
            .with_criteria(
                gko::stop::Iteration::build().with_max_iters(1u).on(ref))
            .on(ref);
    auto dpcpp_solver = dpcpp_idr_factory->generate(d_mtx);
    auto ref_solver = ref_idr_factory->generate(mtx);

    ref_solver->apply(b.get(), x.get());
    dpcpp_solver->apply(d_b.get(), d_x.get());

    GKO_ASSERT_MTX_NEAR(d_b, b, rr<value_type>::value * 10);
    GKO_ASSERT_MTX_NEAR(d_x, x, rr<value_type>::value * 10);
}


}  // namespace