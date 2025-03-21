// SPDX-FileCopyrightText: 2017 - 2024 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GKO_PUBLIC_CORE_LOG_RECORD_HPP_
#define GKO_PUBLIC_CORE_LOG_RECORD_HPP_


#include <deque>
#include <memory>


#include <ginkgo/core/log/logger.hpp>
#include <ginkgo/core/matrix/dense.hpp>
#include <ginkgo/core/stop/criterion.hpp>


namespace gko {

/**
 * @brief The Logging namespace.
 *
 * @ingroup log
 */
namespace log {


/**
 * Struct representing iteration complete related data
 */
struct iteration_complete_data {
    std::unique_ptr<const LinOp> solver;
    std::unique_ptr<const LinOp> right_hand_side;
    std::unique_ptr<const LinOp> solution;
    const size_type num_iterations;
    std::unique_ptr<const LinOp> residual;
    std::unique_ptr<const LinOp> residual_norm;
    std::unique_ptr<const LinOp> implicit_sq_residual_norm;
    array<stopping_status> status;
    bool all_stopped;

    iteration_complete_data(const LinOp* solver, const LinOp* right_hand_side,
                            const LinOp* solution,
                            const size_type num_iterations,
                            const LinOp* residual = nullptr,
                            const LinOp* residual_norm = nullptr,
                            const LinOp* implicit_sq_residual_norm = nullptr,
                            const gko::array<stopping_status>* status = nullptr,
                            bool all_stopped = false)
        : num_iterations{num_iterations}, all_stopped(all_stopped)
    {
        this->solver = solver->clone();
        this->solution = solution->clone();
        if (right_hand_side != nullptr) {
            this->right_hand_side = right_hand_side->clone();
        }
        if (residual != nullptr) {
            this->residual = residual->clone();
        }
        if (residual_norm != nullptr) {
            this->residual_norm = residual_norm->clone();
        }
        if (implicit_sq_residual_norm != nullptr) {
            this->implicit_sq_residual_norm =
                implicit_sq_residual_norm->clone();
        }
        if (status != nullptr) {
            this->status = *status;
        }
    }
};


/**
 * Struct representing Executor related data
 */
struct executor_data {
    const Executor* exec;
    const size_type num_bytes;
    const uintptr location;
};


/**
 * Struct representing Operator related data
 */
struct operation_data {
    const Executor* exec;
    const Operation* operation;
};


/**
 * Struct representing PolymorphicObject related data
 */
struct polymorphic_object_data {
    const Executor* exec;
    std::unique_ptr<const PolymorphicObject> input;
    std::unique_ptr<const PolymorphicObject> output;  // optional

    polymorphic_object_data(const Executor* exec,
                            const PolymorphicObject* input,
                            const PolymorphicObject* output = nullptr)
        : exec{exec}
    {
        this->input = input->clone();
        if (output != nullptr) {
            this->output = output->clone();
        }
    }
};


/**
 * Struct representing LinOp related data
 */
struct linop_data {
    std::unique_ptr<const LinOp> A;
    std::unique_ptr<const LinOp> alpha;
    std::unique_ptr<const LinOp> b;
    std::unique_ptr<const LinOp> beta;
    std::unique_ptr<const LinOp> x;

    linop_data(const LinOp* A, const LinOp* alpha, const LinOp* b,
               const LinOp* beta, const LinOp* x)
    {
        this->A = A->clone();
        if (alpha != nullptr) {
            this->alpha = alpha->clone();
        }
        this->b = b->clone();
        if (beta != nullptr) {
            this->beta = beta->clone();
        }
        this->x = x->clone();
    }
};


/**
 * Struct representing LinOp factory related data
 */
struct linop_factory_data {
    const LinOpFactory* factory;
    std::unique_ptr<const LinOp> input;
    std::unique_ptr<const LinOp> output;

    linop_factory_data(const LinOpFactory* factory, const LinOp* input,
                       const LinOp* output)
        : factory{factory}
    {
        this->input = input->clone();
        if (output != nullptr) {
            this->output = output->clone();
        }
    }
};


/**
 * Struct representing Criterion related data
 */
struct criterion_data {
    const stop::Criterion* criterion;
    const size_type num_iterations;
    std::unique_ptr<const LinOp> residual;
    std::unique_ptr<const LinOp> residual_norm;
    std::unique_ptr<const LinOp> solution;
    const uint8 stopping_id;
    const bool set_finalized;
    const array<stopping_status>* status;
    const bool oneChanged;
    const bool converged;

    criterion_data(const stop::Criterion* criterion,
                   const size_type& num_iterations, const LinOp* residual,
                   const LinOp* residual_norm, const LinOp* solution,
                   const uint8 stopping_id, const bool set_finalized,
                   const array<stopping_status>* status = nullptr,
                   const bool oneChanged = false, const bool converged = false)
        : criterion{criterion},
          num_iterations{num_iterations},
          residual{nullptr},
          residual_norm{nullptr},
          solution{nullptr},
          stopping_id{stopping_id},
          set_finalized{set_finalized},
          status{status},
          oneChanged{oneChanged},
          converged{converged}
    {
        if (residual != nullptr) {
            this->residual = std::unique_ptr<const LinOp>(residual->clone());
        }
        if (residual_norm != nullptr) {
            this->residual_norm =
                std::unique_ptr<const LinOp>(residual_norm->clone());
        }
        if (solution != nullptr) {
            this->solution = std::unique_ptr<const LinOp>(solution->clone());
        }
    }
};


/**
 * Record is a Logger which logs every event to an object. The object can
 * then be accessed at any time by asking the logger to return it.
 *
 * @note Please note that this logger can have significant memory and
 * performance overhead. In particular, when logging events such as the `check`
 * events, all parameters are cloned. If it is sufficient to clone one
 * parameter, consider implementing a specific logger for this. In addition, it
 * is advised to tune the history size in order to control memory overhead.
 */
class Record : public Logger {
public:
    /**
     * Struct storing the actually logged data
     */
    struct logged_data {
        std::deque<std::unique_ptr<executor_data>> allocation_started;
        std::deque<std::unique_ptr<executor_data>> allocation_completed;
        std::deque<std::unique_ptr<executor_data>> free_started;
        std::deque<std::unique_ptr<executor_data>> free_completed;
        std::deque<std::unique_ptr<std::tuple<executor_data, executor_data>>>
            copy_started;
        std::deque<std::unique_ptr<std::tuple<executor_data, executor_data>>>
            copy_completed;

        std::deque<std::unique_ptr<operation_data>> operation_launched;
        std::deque<std::unique_ptr<operation_data>> operation_completed;

        std::deque<std::unique_ptr<polymorphic_object_data>>
            polymorphic_object_create_started;
        std::deque<std::unique_ptr<polymorphic_object_data>>
            polymorphic_object_create_completed;
        std::deque<std::unique_ptr<polymorphic_object_data>>
            polymorphic_object_copy_started;
        std::deque<std::unique_ptr<polymorphic_object_data>>
            polymorphic_object_copy_completed;
        std::deque<std::unique_ptr<polymorphic_object_data>>
            polymorphic_object_move_started;
        std::deque<std::unique_ptr<polymorphic_object_data>>
            polymorphic_object_move_completed;
        std::deque<std::unique_ptr<polymorphic_object_data>>
            polymorphic_object_deleted;

        std::deque<std::unique_ptr<linop_data>> linop_apply_started;
        std::deque<std::unique_ptr<linop_data>> linop_apply_completed;
        std::deque<std::unique_ptr<linop_data>> linop_advanced_apply_started;
        std::deque<std::unique_ptr<linop_data>> linop_advanced_apply_completed;
        std::deque<std::unique_ptr<linop_factory_data>>
            linop_factory_generate_started;
        std::deque<std::unique_ptr<linop_factory_data>>
            linop_factory_generate_completed;

        std::deque<std::unique_ptr<criterion_data>> criterion_check_started;
        std::deque<std::unique_ptr<criterion_data>> criterion_check_completed;

        std::deque<std::unique_ptr<iteration_complete_data>>
            iteration_completed;
    };

    /* Executor events */
    void on_allocation_started(const Executor* exec,
                               const size_type& num_bytes) const override;

    void on_allocation_completed(const Executor* exec,
                                 const size_type& num_bytes,
                                 const uintptr& location) const override;

    void on_free_started(const Executor* exec,
                         const uintptr& location) const override;

    void on_free_completed(const Executor* exec,
                           const uintptr& location) const override;

    void on_copy_started(const Executor* from, const Executor* to,
                         const uintptr& location_from,
                         const uintptr& location_to,
                         const size_type& num_bytes) const override;

    void on_copy_completed(const Executor* from, const Executor* to,
                           const uintptr& location_from,
                           const uintptr& location_to,
                           const size_type& num_bytes) const override;

    /* Operation events */
    void on_operation_launched(const Executor* exec,
                               const Operation* operation) const override;

    void on_operation_completed(const Executor* exec,
                                const Operation* operation) const override;

    /* PolymorphicObject events */
    void on_polymorphic_object_create_started(
        const Executor* exec, const PolymorphicObject* po) const override;

    void on_polymorphic_object_create_completed(
        const Executor* exec, const PolymorphicObject* input,
        const PolymorphicObject* output) const override;

    void on_polymorphic_object_copy_started(
        const Executor* exec, const PolymorphicObject* from,
        const PolymorphicObject* to) const override;

    void on_polymorphic_object_copy_completed(
        const Executor* exec, const PolymorphicObject* from,
        const PolymorphicObject* to) const override;

    void on_polymorphic_object_move_started(
        const Executor* exec, const PolymorphicObject* from,
        const PolymorphicObject* to) const override;

    void on_polymorphic_object_move_completed(
        const Executor* exec, const PolymorphicObject* from,
        const PolymorphicObject* to) const override;

    void on_polymorphic_object_deleted(
        const Executor* exec, const PolymorphicObject* po) const override;

    /* LinOp events */
    void on_linop_apply_started(const LinOp* A, const LinOp* b,
                                const LinOp* x) const override;

    void on_linop_apply_completed(const LinOp* A, const LinOp* b,
                                  const LinOp* x) const override;

    void on_linop_advanced_apply_started(const LinOp* A, const LinOp* alpha,
                                         const LinOp* b, const LinOp* beta,
                                         const LinOp* x) const override;

    void on_linop_advanced_apply_completed(const LinOp* A, const LinOp* alpha,
                                           const LinOp* b, const LinOp* beta,
                                           const LinOp* x) const override;

    /* LinOpFactory events */
    void on_linop_factory_generate_started(const LinOpFactory* factory,
                                           const LinOp* input) const override;

    void on_linop_factory_generate_completed(
        const LinOpFactory* factory, const LinOp* input,
        const LinOp* output) const override;

    /* Criterion events */
    void on_criterion_check_started(const stop::Criterion* criterion,
                                    const size_type& num_iterations,
                                    const LinOp* residual,
                                    const LinOp* residual_norm,
                                    const LinOp* solution,
                                    const uint8& stopping_id,
                                    const bool& set_finalized) const override;

    void on_criterion_check_completed(
        const stop::Criterion* criterion, const size_type& num_iterations,
        const LinOp* residual, const LinOp* residual_norm,
        const LinOp* implicit_residual_norm_sq, const LinOp* solution,
        const uint8& stopping_id, const bool& set_finalized,
        const array<stopping_status>* status, const bool& one_changed,
        const bool& all_converged) const override;

    void on_criterion_check_completed(
        const stop::Criterion* criterion, const size_type& num_iterations,
        const LinOp* residual, const LinOp* residual_norm,
        const LinOp* solution, const uint8& stopping_id,
        const bool& set_finalized, const array<stopping_status>* status,
        const bool& one_changed, const bool& all_converged) const override;

    /* Internal solver events */
    void on_iteration_complete(
        const LinOp* solver, const LinOp* right_hand_side, const LinOp* x,
        const size_type& num_iterations, const LinOp* residual,
        const LinOp* residual_norm, const LinOp* implicit_resnorm_sq,
        const array<stopping_status>* status, bool stopped) const override;

    GKO_DEPRECATED(
        "Please use the version with the additional stopping "
        "information.")
    void on_iteration_complete(const LinOp* solver,
                               const size_type& num_iterations,
                               const LinOp* residual, const LinOp* solution,
                               const LinOp* residual_norm) const override;

    GKO_DEPRECATED(
        "Please use the version with the additional stopping "
        "information.")
    void on_iteration_complete(
        const LinOp* solver, const size_type& num_iterations,
        const LinOp* residual, const LinOp* solution,
        const LinOp* residual_norm,
        const LinOp* implicit_sq_residual_norm) const override;

    /**
     * Creates a Record logger. This dynamically allocates the memory,
     * constructs the object and returns an std::unique_ptr to this object.
     *
     * @param exec  the executor
     * @param enabled_events  the events enabled for this logger. By default all
     *                        events.
     * @param max_storage  the size of storage (i.e. history) wanted by the
     *                     user. By default 0 is used, which means unlimited
     *                     storage. It is advised to control this to reduce
     *                     memory overhead of this logger.
     *
     * @return an std::unique_ptr to the the constructed object
     */
    GKO_DEPRECATED("use two-parameter create")
    static std::unique_ptr<Record> create(
        std::shared_ptr<const Executor> exec,
        const mask_type& enabled_events = Logger::all_events_mask,
        size_type max_storage = 1)
    {
        return std::unique_ptr<Record>(new Record(enabled_events, max_storage));
    }

    /**
     * Creates a Record logger. This dynamically allocates the memory,
     * constructs the object and returns an std::unique_ptr to this object.
     *
     * @param exec  the executor
     * @param enabled_events  the events enabled for this logger. By default all
     *                        events.
     * @param max_storage  the size of storage (i.e. history) wanted by the
     *                     user. By default 0 is used, which means unlimited
     *                     storage. It is advised to control this to reduce
     *                     memory overhead of this logger.
     *
     * @return an std::unique_ptr to the the constructed object
     */
    static std::unique_ptr<Record> create(
        const mask_type& enabled_events = Logger::all_events_mask,
        size_type max_storage = 1)
    {
        return std::unique_ptr<Record>(new Record(enabled_events, max_storage));
    }

    /**
     * Returns the logged data
     *
     * @return the logged data
     */
    const logged_data& get() const noexcept { return data_; }

    /**
     * @copydoc ::get()
     */
    logged_data& get() noexcept { return data_; }

protected:
    /**
     * Creates a Record logger.
     *
     * @param exec  the executor
     * @param enabled_events  the events enabled for this logger. By default all
     *                        events.
     * @param max_storage  the size of storage (i.e. history) wanted by the
     *                     user. By default 0 is used, which means unlimited
     *                     storage. It is advised to control this to reduce
     *                     memory overhead of this logger.
     */
    GKO_DEPRECATED("use two-parameter constructor")
    explicit Record(std::shared_ptr<const gko::Executor> exec,
                    const mask_type& enabled_events = Logger::all_events_mask,
                    size_type max_storage = 0)
        : Record(enabled_events, max_storage)
    {}

    /**
     * Creates a Record logger.
     *
     * @param enabled_events  the events enabled for this logger. By default all
     *                        events.
     * @param max_storage  the size of storage (i.e. history) wanted by the
     *                     user. By default 0 is used, which means unlimited
     *                     storage. It is advised to control this to reduce
     *                     memory overhead of this logger.
     */
    explicit Record(const mask_type& enabled_events = Logger::all_events_mask,
                    size_type max_storage = 0)
        : Logger(enabled_events), max_storage_{max_storage}
    {}

    /**
     * Helper function which appends an object to a deque
     *
     * @tparam deque_type  the type of objects in the deque
     *
     * @param deque  the deque to append the object to
     * @param object  the object to append
     */
    template <typename deque_type>
    void append_deque(std::deque<deque_type>& deque, deque_type object) const
    {
        if (this->max_storage_ && deque.size() == this->max_storage_) {
            deque.pop_front();
        }
        deque.push_back(std::move(object));
    }

private:
    mutable logged_data data_{};
    size_type max_storage_{};
};


}  // namespace log
}  // namespace gko


#endif  // GKO_PUBLIC_CORE_LOG_RECORD_HPP_
