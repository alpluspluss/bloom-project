/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <tuple>

namespace blm
{
	/**
	 * @brief Status returned by behavior tree nodes
	 */
	enum class BTStatus
	{
		SUCCESS,
		FAILURE,
		CONTINUE
	};

	/**
	 * @brief Concept defining requirements for behavior tree context
	 */
	template<typename ContextType>
	concept BTContext = requires(ContextType ctx)
	{
		{ ctx.mark_changed() } -> std::convertible_to<void>;
		{ ctx.has_changed() } -> std::convertible_to<bool>;
		{ ctx.reset_changed() } -> std::convertible_to<void>;
	};

	/**
	 * @brief Concept defining requirements for behavior tree nodes
	 */
	template<typename T, typename ContextType>
	concept BTNode = requires(T node, ContextType &ctx)
	{
		{ node.execute(ctx) } -> std::convertible_to<BTStatus>;
	} && BTContext<ContextType>;

	/**
	 * @brief Pattern matching node for condition checking
	 */
	template<typename Func, typename ContextType>
	class BTPattern
	{
	public:
		constexpr explicit BTPattern(Func f) : predicate(std::move(f)) {}

		constexpr BTStatus execute(ContextType &ctx) const
		{
			return predicate(ctx) ? BTStatus::SUCCESS : BTStatus::FAILURE;
		}

		constexpr void reset() const noexcept {}

	private:
		Func predicate;
	};

	/**
	 * @brief Transform node for performing actions
	 */
	template<typename Func, typename ContextType>
	class BTTransform
	{
	public:
		constexpr explicit BTTransform(Func f) : action(std::move(f)) {}

		constexpr BTStatus execute(ContextType &ctx) const
		{
			return action(ctx);
		}

		constexpr void reset() const noexcept {}

	private:
		Func action;
	};

	/**
	 * @brief Rule node combining pattern and transform
	 */
	template<typename PatternType, typename TransformType, typename ContextType>
	class BTRule
	{
	public:
		constexpr BTRule(PatternType p, TransformType t) : pattern(std::move(p)), transform(std::move(t)) {}

		constexpr BTStatus execute(ContextType &ctx) const
		{
			if (pattern.execute(ctx) == BTStatus::SUCCESS)
			{
				return transform.execute(ctx);
			}
			return BTStatus::FAILURE;
		}

		constexpr void reset() const noexcept
		{
			pattern.reset();
			transform.reset();
		}

	private:
		PatternType pattern;
		TransformType transform;
	};

	/**
	 * @brief selector node; tries children until one succeeds
	 */
	template<typename... Children>
	class BTSelector
	{
	public:
		constexpr explicit BTSelector(Children... args) : children(std::move(args)...) {}

		template<typename ContextType>
		constexpr BTStatus execute(ContextType &ctx) const
		{
			return execute_children(ctx);
		}

		constexpr void reset() const noexcept
		{
			reset_children();
		}

	private:
		std::tuple<Children...> children;

		template<std::size_t I = 0, typename ContextType>
		constexpr BTStatus execute_children(ContextType &ctx) const
		{
			if constexpr (I < sizeof...(Children))
			{
				BTStatus result = std::get<I>(children).execute(ctx);
				if (result == BTStatus::SUCCESS || result == BTStatus::CONTINUE)
				{
					return result;
				}
				return execute_children<I + 1>(ctx);
			}
			else
			{
				return BTStatus::FAILURE;
			}
		}

		template<std::size_t I = 0>
		constexpr void reset_children() const
		{
			if constexpr (I < sizeof...(Children))
			{
				std::get<I>(children).reset();
				reset_children<I + 1>();
			}
		}
	};

	/**
	 * @brief Sequence node; all children must succeed in order
	 */
	template<typename... Children>
	class BTSequence
	{
	public:
		constexpr explicit BTSequence(Children... args) : children(std::move(args)...) {}

		template<typename ContextType>
		constexpr BTStatus execute(ContextType &ctx) const
		{
			return execute_children(ctx);
		}

		constexpr void reset() const noexcept
		{
			reset_children();
		}

	private:
		std::tuple<Children...> children;

		template<std::size_t I = 0, typename ContextType>
		constexpr BTStatus execute_children(ContextType &ctx) const
		{
			if constexpr (I < sizeof...(Children))
			{
				BTStatus result = std::get<I>(children).execute(ctx);
				if (result == BTStatus::FAILURE)
				{
					return BTStatus::FAILURE;
				}
				if (result == BTStatus::CONTINUE)
				{
					return BTStatus::CONTINUE;
				}
				return execute_children<I + 1>(ctx);
			}
			else
			{
				return BTStatus::SUCCESS;
			}
		}

		template<std::size_t I = 0>
		constexpr void reset_children() const
		{
			if constexpr (I < sizeof...(Children))
			{
				std::get<I>(children).reset();
				reset_children<I + 1>();
			}
		}
	};

	/**
	 * @brief Parallel node; executes all children
	 */
	template<typename... Children>
	class BTParallel
	{
	public:
		constexpr explicit BTParallel(Children... args) : children(std::move(args)...) {}

		template<typename ContextType>
		constexpr BTStatus execute(ContextType &ctx) const
		{
			auto [any_success, any_continue] = execute_children(ctx);
			if (any_continue)
				return BTStatus::CONTINUE;
			return any_success ? BTStatus::SUCCESS : BTStatus::FAILURE;
		}

		constexpr void reset() const noexcept
		{
			reset_children();
		}

	private:
		std::tuple<Children...> children;

		template<std::size_t I = 0, typename ContextType>
		constexpr std::pair<bool, bool> execute_children(ContextType &ctx) const
		{
			if constexpr (I < sizeof...(Children))
			{
				BTStatus result = std::get<I>(children).execute(ctx);
				auto [any_success, any_continue] = execute_children<I + 1>(ctx);

				return {
					any_success || (result == BTStatus::SUCCESS),
					any_continue || (result == BTStatus::CONTINUE)
				};
			}
			else
			{
				return { false, false };
			}
		}

		template<std::size_t I = 0>
		constexpr void reset_children() const
		{
			if constexpr (I < sizeof...(Children))
			{
				std::get<I>(children).reset();
				reset_children<I + 1>();
			}
		}
	};

	/**
	 * @brief Fixpoint node - repeats child until no changes occur
	 */
	template<typename Child>
	class BTFixpoint
	{
	public:
		constexpr BTFixpoint(Child c, int max_iter = 1000) : child(std::move(c)), max_iterations(max_iter) {}

		template<typename ContextType>
		constexpr BTStatus execute(ContextType &ctx) const
		{
			bool made_progress = false;

			for (int i = 0; i < max_iterations; ++i)
			{
				ctx.reset_changed();
				BTStatus result = child.execute(ctx);

				if (result == BTStatus::CONTINUE)
				{
					return BTStatus::CONTINUE;
				}

				if (!ctx.has_changed())
				{
					break; /* reached fixpoint */
				}

				made_progress = true;
				child.reset();
			}

			return made_progress ? BTStatus::SUCCESS : BTStatus::FAILURE;
		}

		constexpr void reset() const noexcept
		{
			child.reset();
		}

	private:
		Child child;
		int max_iterations;
	};

	/**
	 * @brief Inverter node; inverts child result
	 */
	template<typename Child>
	class BTInvert
	{
	public:
		constexpr explicit BTInvert(Child c) : child(std::move(c)) {}

		template<typename ContextType>
		constexpr BTStatus execute(ContextType &ctx) const
		{
			BTStatus result = child.execute(ctx);
			if (result == BTStatus::SUCCESS)
				return BTStatus::FAILURE;
			if (result == BTStatus::FAILURE)
				return BTStatus::SUCCESS;
			return BTStatus::CONTINUE;
		}

		constexpr void reset() const noexcept
		{
			child.reset();
		}

	private:
		Child child;
	};

	/**
	 * @brief Wrapper for pattern functions to enable type deduction
	 */
	template<typename Func>
	class BTPatternWrapper
	{
	public:
		constexpr explicit BTPatternWrapper(Func f) : predicate(std::move(f)) {}

		template<typename ContextType>
		constexpr BTStatus execute(ContextType &ctx) const
		{
			return predicate(ctx) ? BTStatus::SUCCESS : BTStatus::FAILURE;
		}

		constexpr void reset() const noexcept {}

	private:
		Func predicate;
	};

	/**
	 * @brief Wrapper for transform functions to enable type deduction
	 */
	template<typename Func>
	class BTTransformWrapper
	{
	public:
		constexpr explicit BTTransformWrapper(Func f) : action(std::move(f)) {}

		template<typename ContextType>
		constexpr BTStatus execute(ContextType &ctx) const
		{
			return action(ctx);
		}

		constexpr void reset() const noexcept {}

	private:
		Func action;
	};

	/**
	 * @brief Wrapper for rule combinations to enable type deduction
	 */
	template<typename PatternType, typename TransformType>
	class BTRuleWrapper
	{
	public:
		constexpr BTRuleWrapper(PatternType p, TransformType t) : pattern(std::move(p)), transform(std::move(t)) {}

		template<typename ContextType>
		constexpr BTStatus execute(ContextType &ctx) const
		{
			if (pattern.execute(ctx) == BTStatus::SUCCESS)
			{
				return transform.execute(ctx);
			}
			return BTStatus::FAILURE;
		}

		constexpr void reset() const noexcept
		{
			pattern.reset();
			transform.reset();
		}

	private:
		PatternType pattern;
		TransformType transform;
	};

	/**
	 * @brief Static factory class for creating behavior tree nodes
	 */
	class BT
	{
	public:
		/**
		 * @brief Create a pattern node for condition checking
		 */
		template<typename Func>
		static constexpr auto pattern(Func &&f)
		{
			return BTPatternWrapper<std::decay_t<Func> > { std::forward<Func>(f) };
		}

		/**
		 * @brief Create a transform node for performing actions
		 */
		template<typename Func>
		static constexpr auto transform(Func &&f)
		{
			return BTTransformWrapper<std::decay_t<Func> > { std::forward<Func>(f) };
		}

		/**
		 * @brief Create a rule node combining pattern and transform
		 */
		template<typename PatternType, typename TransformType>
		static constexpr auto rule(PatternType &&p, TransformType &&t)
		{
			return BTRuleWrapper<std::decay_t<PatternType>, std::decay_t<TransformType> > {
				std::forward<PatternType>(p), std::forward<TransformType>(t)
			};
		}

		/**
		 * @brief Create a selector node; tries children until one succeeds
		 */
		template<typename... Children>
		static constexpr auto selector(Children &&... children)
		{
			return BTSelector<std::decay_t<Children>...> { std::forward<Children>(children)... };
		}

		/**
		 * @brief Create a sequence node; all children must succeed in order
		 */
		template<typename... Children>
		static constexpr auto sequence(Children &&... children)
		{
			return BTSequence<std::decay_t<Children>...> { std::forward<Children>(children)... };
		}

		/**
		 * @brief Create a parallel node; executes all children
		 */
		template<typename... Children>
		static constexpr auto parallel(Children &&... children)
		{
			return BTParallel<std::decay_t<Children>...> { std::forward<Children>(children)... };
		}

		/**
		 * @brief Create a fixpoint node; repeats child until no changes occur
		 */
		template<typename Child>
		static constexpr auto fixpoint(Child &&child, int max_iter = 1000)
		{
			return BTFixpoint<std::decay_t<Child> > { std::forward<Child>(child), max_iter };
		}

		/**
		 * @brief Create an inverter node; inverts child result
		 */
		template<typename Child>
		static constexpr auto invert(Child &&child)
		{
			return BTInvert<std::decay_t<Child> > { std::forward<Child>(child) };
		}
	};
}
