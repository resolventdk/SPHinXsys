/* -------------------------------------------------------------------------*
 *								SPHinXsys									*
 * --------------------------------------------------------------------------*
 * SPHinXsys (pronunciation: s'finksis) is an acronym from Smoothed Particle	*
 * Hydrodynamics for industrial compleX systems. It provides C++ APIs for	*
 * physical accurate simulation and aims to model coupled industrial dynamic *
 * systems including fluid, solid, multi-body dynamics and beyond with SPH	*
 * (smoothed particle hydrodynamics), a meshless computational method using	*
 * particle discretization.													*
 *																			*
 * SPHinXsys is partially funded by German Research Foundation				*
 * (Deutsche Forschungsgemeinschaft) DFG HU1527/6-1, HU1527/10-1				*
 * and HU1527/12-1.															*
 *                                                                           *
 * Portions copyright (c) 2017-2020 Technical University of Munich and		*
 * the authors' affiliations.												*
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may   *
 * not use this file except in compliance with the License. You may obtain a *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.        *
 *                                                                           *
 * --------------------------------------------------------------------------*/
/**
 * @file base_particle_dynamics.h
 * @brief This is for the base classes of particle dynamics, which describe the
 * interaction between particles. These interactions are used to define
 * differential operators for surface forces or fluxes in continuum mechanics
 * @author  Xiangyu Hu, Luhui Han and Chi Zhang
 */

#ifndef BASE_PARTICLE_DYNAMICS_H
#define BASE_PARTICLE_DYNAMICS_H

#include "base_data_package.h"
#include "sph_data_containers.h"
#include "neighbor_relation.h"
#include "body_relation.h"
#include "base_body.h"

#include <functional>

using namespace std::placeholders;

namespace SPH
{

	/** Functor for operation on particles. */
	typedef std::function<void(size_t, Real)> ParticleFunctor;
	/** Functors for reducing operation on particles. */
	template <class ReturnType>
	using ReduceFunctor = std::function<ReturnType(size_t, Real)>;

	/** Iterators for particle functors. sequential computing. */
	void ParticleIterator(size_t total_real_particles, const ParticleFunctor &particle_functor, Real dt = 0.0);
	/** Iterators for particle functors. parallel computing. */
	void ParticleIterator_parallel(size_t total_real_particles, const ParticleFunctor &particle_functor, Real dt = 0.0);

	/** Iterators for reduce functors. sequential computing. */
	template <class ReturnType, typename ReduceOperation>
	ReturnType ReduceIterator(size_t total_real_particles, ReturnType temp,
							  ReduceFunctor<ReturnType> &reduce_functor, ReduceOperation &reduce_operation, Real dt = 0.0);
	/** Iterators for reduce functors. parallel computing. */
	template <class ReturnType, typename ReduceOperation>
	ReturnType ReduceIterator_parallel(size_t total_real_particles, ReturnType temp,
									   ReduceFunctor<ReturnType> &reduce_functor, ReduceOperation &reduce_operation, Real dt = 0.0);

	/** Iterators for particle functors with splitting. sequential computing. */
	void ParticleIteratorSplittingSweep(SplitCellLists &split_cell_lists,
										const ParticleFunctor &particle_functor, Real dt = 0.0);
	/** Iterators for particle functors with splitting. parallel computing. */
	void ParticleIteratorSplittingSweep_parallel(SplitCellLists &split_cell_lists,
												 const ParticleFunctor &particle_functor, Real dt = 0.0);

	/** Functor for operation on particles. */
	typedef std::function<void(const blocked_range<size_t> &, Real)> ParticleRangeFunctor;
	/** Iterators for particle functors. sequential computing. */
	void ParticleIterator(size_t total_real_particles, const ParticleRangeFunctor &particle_functor, Real dt = 0.0);
	/** Iterators for particle functors. parallel computing. */
	void ParticleIterator_parallel(size_t total_real_particles, const ParticleRangeFunctor &particle_functor, Real dt = 0.0);

	/** A Functor for Summation */
	template <class ReturnType>
	struct ReduceSum
	{
		ReturnType operator()(const ReturnType &x, const ReturnType &y) const { return x + y; };
	};
	/** A Functor for Maximum */
	struct ReduceMax
	{
		Real operator()(Real x, Real y) const { return SMAX(x, y); };
	};
	/** A Functor for Minimum */
	struct ReduceMin
	{
		Real operator()(Real x, Real y) const { return SMIN(x, y); };
	};
	/** A Functor for OR operator */
	struct ReduceOR
	{
		bool operator()(bool x, bool y) const { return x || y; };
	};
	/** A Functor for AND operator */
	struct ReduceAND
	{
		bool operator()(bool x, bool y) const { return x && y; };
	};
	/** A Functor for lower bound */
	struct ReduceLowerBound
	{
		Vecd operator()(const Vecd &x, const Vecd &y) const
		{
			Vecd lower_bound;
			for (int i = 0; i < lower_bound.size(); ++i)
				lower_bound[i] = SMIN(x[i], y[i]);
			return lower_bound;
		};
	};
	/** A Functor for upper bound */
	struct ReduceUpperBound
	{
		Vecd operator()(const Vecd &x, const Vecd &y) const
		{
			Vecd upper_bound;
			for (int i = 0; i < upper_bound.size(); ++i)
				upper_bound[i] = SMAX(x[i], y[i]);
			return upper_bound;
		};
	};

	/**
	 * @class GlobalStaticVariables
	 * @brief A place to put all global variables
	 */
	class GlobalStaticVariables
	{
	public:
		explicit GlobalStaticVariables(){};
		virtual ~GlobalStaticVariables(){};

		/** the physical time is global value for all dynamics */
		static Real physical_time_;
	};

	/**
	 * @class BaseParticleDynamics
	 * @brief The new version of base class for all particle dynamics
	 * This class contains the only two interface functions available
	 * for particle dynamics. An specific implementation should be realized.
	 */
	template <class ReturnType = void>
	class BaseParticleDynamics : public GlobalStaticVariables
	{
	public:
		explicit BaseParticleDynamics(){};
		virtual ~BaseParticleDynamics(){};

		/** The only two functions can be called from outside
		 * One is for sequential execution, the other is for parallel. */
		virtual ReturnType exec(Real dt = 0.0) = 0;
		virtual ReturnType parallel_exec(Real dt = 0.0) = 0;
	};

	/**
	 * @class ParticleDynamics
	 * @brief The basic particle dynamics in which a range of particles are looped.
	 */
	template <typename LoopRange, typename ParticleFunctorType = ParticleFunctor>
	class ParticleDynamics : public BaseParticleDynamics<void>
	{
	public:
		ParticleDynamics(LoopRange &loop_range, ParticleFunctorType particle_functor)
			: BaseParticleDynamics<void>(),
			  loop_range_(loop_range), particle_functor_(particle_functor){};

		virtual ~ParticleDynamics(){};

		virtual void exec(Real dt = 0.0) override
		{
			ParticleIterator(loop_range_, particle_functor_, dt);
		};

		virtual void parallel_exec(Real dt = 0.0) override
		{
			ParticleIterator_parallel(loop_range_, particle_functor_, dt);
		};

	protected:
		LoopRange &loop_range_;
		ParticleFunctorType particle_functor_;
	};

	/**
	 * @class BaseInteractionDynamics
	 * @brief  This is the class for particle interaction with other particles
	 */
	template <typename LoopRange>
	class BaseInteractionDynamics : public BaseParticleDynamics<void>
	{
		ParticleDynamics<LoopRange, ParticleFunctor> interaction_dynamics_;
		/** pre process such as update ghost state */
		StdVec<BaseParticleDynamics<void> *> pre_processes_;
		/** post process such as impose constraint */
		StdVec<BaseParticleDynamics<void> *> post_processes_;

	public:
		BaseInteractionDynamics(LoopRange &loop_range, ParticleFunctor functor_interaction)
			: BaseParticleDynamics<void>(), interaction_dynamics_(loop_range, functor_interaction){};

		virtual ~BaseInteractionDynamics(){};

		void addPreProcess(BaseParticleDynamics<void> *pre_process) { pre_processes_.push_back(pre_process); };
		void addPostProcess(BaseParticleDynamics<void> *post_process) { post_processes_.push_back(post_process); };

		virtual void exec(Real dt = 0.0) override
		{
			runSetup(dt);
			runInteraction(dt);
		};

		virtual void parallel_exec(Real dt = 0.0) override
		{
			runSetup(dt);
			runInteraction_parallel(dt);
		};

		virtual void runSetup(Real dt = 0.0) = 0;

		void runInteraction(Real dt = 0.0)
		{
			for (size_t k = 0; k < pre_processes_.size(); ++k)
				pre_processes_[k]->exec(dt);
			interaction_dynamics_.exec(dt);
			for (size_t k = 0; k < post_processes_.size(); ++k)
				post_processes_[k]->exec(dt);
		};

		void runInteraction_parallel(Real dt = 0.0)
		{
			for (size_t k = 0; k < pre_processes_.size(); ++k)
				pre_processes_[k]->parallel_exec(dt);
			interaction_dynamics_.parallel_exec(dt);
			for (size_t k = 0; k < post_processes_.size(); ++k)
				post_processes_[k]->parallel_exec(dt);
		};
	};

	/**
	 * @class BaseInteractionDynamicsWithUpdate
	 * @brief This class includes an interaction and a update steps
	 */
	template <typename LoopRange, typename ParticleFunctorType>
	class BaseInteractionDynamicsWithUpdate : public BaseInteractionDynamics<LoopRange>
	{
		ParticleDynamics<LoopRange, ParticleFunctorType> update_dynamics_;

	public:
		BaseInteractionDynamicsWithUpdate(LoopRange &loop_range, ParticleFunctor functor_interaction,
										  ParticleFunctorType functor_update)
			: BaseInteractionDynamics<LoopRange>(loop_range, functor_interaction),
			  update_dynamics_(loop_range, functor_update){};
		virtual ~BaseInteractionDynamicsWithUpdate(){};

		virtual void exec(Real dt = 0.0) override
		{
			this->runSetup(dt);
			BaseInteractionDynamics<LoopRange>::runInteraction(dt);
			runUpdate(dt);
		};

		virtual void parallel_exec(Real dt = 0.0) override
		{
			this->runSetup(dt);
			BaseInteractionDynamics<LoopRange>::runInteraction_parallel(dt);
			runUpdate_parallel(dt);
		};

		void runUpdate(Real dt = 0.0)
		{
			update_dynamics_.exec(dt);
		};

		void runUpdate_parallel(Real dt = 0.0)
		{
			update_dynamics_.parallel_exec(dt);
		};
	};

	/**
	 * @class BaseInteractionDynamics1Level
	 * @brief This class includes an initialization, an interaction and a update steps
	 */
	template <typename LoopRange, typename ParticleFunctorType>
	class BaseInteractionDynamics1Level : public BaseInteractionDynamicsWithUpdate<LoopRange, ParticleFunctorType>
	{
		ParticleDynamics<LoopRange, ParticleFunctorType> initialize_dynamics_;

	public:
		BaseInteractionDynamics1Level(LoopRange &loop_range, ParticleFunctorType functor_initialization,
									  ParticleFunctor functor_interaction, ParticleFunctorType functor_update)
			: BaseInteractionDynamicsWithUpdate<LoopRange, ParticleFunctorType>(loop_range, functor_interaction, functor_update),
			  initialize_dynamics_(loop_range, functor_initialization){};
		virtual ~BaseInteractionDynamics1Level(){};

		virtual void exec(Real dt = 0.0) override
		{
			this->runSetup(dt);
			runInitialization(dt);
			BaseInteractionDynamics<LoopRange>::runInteraction(dt);
			BaseInteractionDynamicsWithUpdate<LoopRange, ParticleFunctorType>::runUpdate(dt);
		};

		virtual void parallel_exec(Real dt = 0.0) override
		{
			this->runSetup(dt);
			runInitialization_parallel(dt);
			BaseInteractionDynamics<LoopRange>::runInteraction_parallel(dt);
			BaseInteractionDynamicsWithUpdate<LoopRange, ParticleFunctorType>::runUpdate_parallel(dt);
		};

		void runInitialization(Real dt = 0.0)
		{
			initialize_dynamics_.exec(dt);
		};

		void runInitialization_parallel(Real dt = 0.0)
		{
			initialize_dynamics_.parallel_exec(dt);
		};
	};

	/**
	 * @class LocalParticleDynamics
	 * @brief The new version of base class for all local particle dynamics.
	 */
	class LocalParticleDynamics
	{
		SPHBody *sph_body_;

	public:
		explicit LocalParticleDynamics(SPHBody &sph_body) : sph_body_(&sph_body){};
		virtual ~LocalParticleDynamics(){};

		void setBodyUpdated() { sph_body_->setNewlyUpdated(); };
		/** the function for set global parameters for the particle dynamics */
		virtual void setupDynamics(Real dt = 0.0){};
	};

	/**
	 * @class OldParticleDynamics
	 * @brief The base class for all particle dynamics
	 * This class contains the only two interface functions available
	 * for particle dynamics. An specific implementation should be realized.
	 */
	template <class ReturnType = void>
	class OldParticleDynamics : public GlobalStaticVariables
	{
	public:
		explicit OldParticleDynamics(SPHBody &sph_body);
		virtual ~OldParticleDynamics(){};

		SPHBody *getSPHBody() { return sph_body_; };
		/** The only two functions can be called from outside
		 * One is for sequential execution, the other is for parallel. */
		virtual ReturnType exec(Real dt = 0.0) = 0;
		virtual ReturnType parallel_exec(Real dt = 0.0) = 0;

	protected:
		SPHBody *sph_body_;
		SPHAdaptation *sph_adaptation_;
		BaseParticles *base_particles_;

		void setBodyUpdated() { sph_body_->setNewlyUpdated(); };
		/** the function for set global parameters for the particle dynamics */
		virtual void setupDynamics(Real dt = 0.0){};
	};

	/**
	 * @class DataDelegateBase
	 * @brief empty base class mixin template.
	 */
	class DataDelegateEmptyBase
	{
	public:
		explicit DataDelegateEmptyBase(SPHBody &sph_body){};
		virtual ~DataDelegateEmptyBase(){};
	};

	/**
	 * @class DataDelegateSimple
	 * @brief prepare data for simple particle dynamics.
	 */
	template <class BodyType = SPHBody,
			  class ParticlesType = BaseParticles,
			  class MaterialType = BaseMaterial>
	class DataDelegateSimple
	{
	public:
		explicit DataDelegateSimple(SPHBody &sph_body)
			: body_(DynamicCast<BodyType>(this, &sph_body)),
			  particles_(DynamicCast<ParticlesType>(this, sph_body.base_particles_)),
			  material_(DynamicCast<MaterialType>(this, sph_body.base_material_)),
			  sorted_id_(sph_body.base_particles_->sorted_id_),
			  unsorted_id_(sph_body.base_particles_->unsorted_id_){};
		virtual ~DataDelegateSimple(){};

		BodyType *getBody() { return body_; };
		ParticlesType *getParticles() { return particles_; };
		MaterialType *getMaterial() { return material_; };

	protected:
		BodyType *body_;
		ParticlesType *particles_;
		MaterialType *material_;
		StdLargeVec<size_t> &sorted_id_;
		StdLargeVec<size_t> &unsorted_id_;
	};

	/**
	 * @class DataDelegateInner
	 * @brief prepare data for inner particle dynamics
	 */
	template <class BodyType = SPHBody,
			  class ParticlesType = BaseParticles,
			  class MaterialType = BaseMaterial,
			  class BaseDataDelegateType = DataDelegateSimple<BodyType, ParticlesType, MaterialType>>
	class DataDelegateInner : public BaseDataDelegateType
	{
	public:
		explicit DataDelegateInner(BaseBodyRelationInner &body_inner_relation)
			: BaseDataDelegateType(*body_inner_relation.sph_body_),
			  inner_configuration_(body_inner_relation.inner_configuration_){};
		virtual ~DataDelegateInner(){};

	protected:
		/** inner configuration of the designated body */
		ParticleConfiguration &inner_configuration_;
	};

	/**
	 * @class DataDelegateContact
	 * @brief prepare data for contact particle dynamics
	 */
	template <class BodyType = SPHBody,
			  class ParticlesType = BaseParticles,
			  class MaterialType = BaseMaterial,
			  class ContactBodyType = SPHBody,
			  class ContactParticlesType = BaseParticles,
			  class ContactMaterialType = BaseMaterial,
			  class BaseDataDelegateType = DataDelegateSimple<BodyType, ParticlesType, MaterialType>>
	class DataDelegateContact : public BaseDataDelegateType
	{
	public:
		explicit DataDelegateContact(BaseBodyRelationContact &body_contact_relation);
		virtual ~DataDelegateContact(){};

	protected:
		StdVec<ContactBodyType *> contact_bodies_;
		StdVec<ContactParticlesType *> contact_particles_;
		StdVec<ContactMaterialType *> contact_material_;
		/** Configurations for particle interaction between bodies. */
		StdVec<ParticleConfiguration *> contact_configuration_;
	};

	/**
	 * @class DataDelegateComplex
	 * @brief prepare data for complex particle dynamics
	 */
	template <class BodyType = SPHBody,
			  class ParticlesType = BaseParticles,
			  class MaterialType = BaseMaterial,
			  class ContactBodyType = SPHBody,
			  class ContactParticlesType = BaseParticles,
			  class ContactMaterialType = BaseMaterial>
	class DataDelegateComplex : public DataDelegateInner<BodyType, ParticlesType, MaterialType>,
								public DataDelegateContact<BodyType, ParticlesType, MaterialType,
														   ContactBodyType, ContactParticlesType, ContactMaterialType, DataDelegateEmptyBase>
	{
	public:
		explicit DataDelegateComplex(ComplexBodyRelation &body_complex_relation)
			: DataDelegateInner<BodyType, ParticlesType, MaterialType>(body_complex_relation.inner_relation_),
			  DataDelegateContact<BodyType, ParticlesType, MaterialType, ContactBodyType, ContactParticlesType,
								  ContactMaterialType, DataDelegateEmptyBase>(body_complex_relation.contact_relation_){};
		virtual ~DataDelegateComplex(){};
	};

	/**
	 * @class ParticleDynamicsComplex
	 * @brief particle dynamics by considering  contribution from extra contact bodies
	 */
	template <class ParticleDynamicsInnerType, class ContactDataType>
	class ParticleDynamicsComplex : public ParticleDynamicsInnerType, public ContactDataType
	{
	public:
		ParticleDynamicsComplex(BaseBodyRelationInner &inner_relation,
								BaseBodyRelationContact &contact_relation)
			: ParticleDynamicsInnerType(inner_relation), ContactDataType(contact_relation){};

		ParticleDynamicsComplex(ComplexBodyRelation &complex_relation,
								BaseBodyRelationContact &extra_contact_relation);

		virtual ~ParticleDynamicsComplex(){};

	protected:
		virtual void prepareContactData() = 0;
	};
}
#endif // BASE_PARTICLE_DYNAMICS_H