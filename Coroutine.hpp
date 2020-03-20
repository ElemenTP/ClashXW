/*
 * Copyright (C) 2020 Richard Yu <yurichard3839@gmail.com>
 *
 * This file is part of ClashW.
 *
 * ClashW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ClashW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with ClashW.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

template <typename T, typename TExecutor>
[[nodiscard]] inline auto Promise(TExecutor&& executor) noexcept
{
	using std::experimental::coroutine_handle;

	struct _Promise
	{
		_Promise(TExecutor&& executor) : m_coroHandle(nullptr), m_result(std::nullopt), m_reason()
		{
			executor([this](T&& resolution) {
				assert(!m_result.has_value());
				assert(!m_reason);

				m_result.emplace(std::move(resolution));

				if (m_coroHandle)
					coroutine_handle<>::from_address(m_coroHandle).resume();
			}, [this](const std::exception_ptr& reason) {
				assert(!m_result.has_value());
				assert(!m_reason);

				m_reason = reason;

				if (m_coroHandle)
					coroutine_handle<>::from_address(m_coroHandle).resume();
			});
		}

		struct awaiter_base
		{
			awaiter_base(_Promise* self) noexcept : m_self(self) {}

			bool await_ready() const noexcept
			{
				return m_self->m_reason || m_self->m_result.has_value();
			}

			void await_suspend(coroutine_handle<> handle)
			{
				m_self->m_coroHandle = handle.address();
			}

		protected:
			_Promise* const m_self;
		};

		auto operator co_await() & noexcept
		{
			struct awaiter : awaiter_base
			{
				decltype(auto) await_resume() const
				{
					if (m_self->m_reason)
						std::rethrow_exception(m_self->m_reason);

					assert(m_self->m_result.has_value());
					return *(m_self->m_result);
				}
			};

			return awaiter{ this };
		}

		auto operator co_await() && noexcept
		{
			struct awaiter : awaiter_base
			{
				decltype(auto) await_resume() const
				{
					if (m_self->m_reason)
						std::rethrow_exception(m_self->m_reason);

					assert(m_self->m_result.has_value());
					return *std::move(m_self->m_result);
				}
			};

			return awaiter{ this };
		}

	private:
		void* m_coroHandle;
		std::optional<T> m_result;
		std::exception_ptr m_reason;
	};

	return _Promise{ std::forward<TExecutor>(executor) };
}

template <typename T, typename TExecutor>
[[nodiscard]] inline auto LazyPromise(TExecutor&& executor) noexcept
{
	using std::experimental::coroutine_handle;

	struct _LazyPromise
	{
		_LazyPromise(TExecutor&& executor) : m_executor(std::move(executor)),
			m_coroHandle(nullptr), m_result(std::nullopt), m_reason() {}

		struct awaiter_base
		{
			awaiter_base(_LazyPromise* self) noexcept : m_self(self) {}

			bool await_ready() const noexcept
			{
				return m_self->m_reason || m_self->m_result.has_value();
			}

			void await_suspend(coroutine_handle<> handle)
			{
				m_self->m_coroHandle = handle.address();
				m_self->m_executor([this](T&& resolution) {
					assert(!m_self->m_result.has_value());
					assert(!m_self->m_reason);

					m_self->m_result.emplace(std::move(resolution));

					coroutine_handle<>::from_address(m_self->m_coroHandle).resume();
				}, [this](const std::exception_ptr& reason) {
					assert(!m_self->m_result.has_value());
					assert(!m_self->m_reason);

					m_self->m_reason = reason;

					coroutine_handle<>::from_address(m_self->m_coroHandle).resume();
				});
			}

		protected:
			_LazyPromise* const m_self;
		};

		auto operator co_await() & noexcept
		{
			struct awaiter : awaiter_base
			{
				decltype(auto) await_resume() const
				{
					if (m_self->m_reason)
						std::rethrow_exception(m_self->m_reason);

					assert(m_self->m_result.has_value());
					return *(m_self->m_result);
				}
			};

			return awaiter{ this };
		}

		auto operator co_await() && noexcept
		{
			struct awaiter : awaiter_base
			{
				decltype(auto) await_resume() const
				{
					if (m_self->m_reason)
						std::rethrow_exception(m_self->m_reason);

					assert(m_self->m_result.has_value());
					return *std::move(m_self->m_result);
				}
			};

			return awaiter{ this };
		}

	private:
		TExecutor m_executor;
		void* m_coroHandle;
		std::optional<T> m_result;
		std::exception_ptr m_reason;
	};

	return _LazyPromise{ std::forward<TExecutor>(executor) };
}

[[nodiscard]] inline auto ResumeForeground() noexcept
{
	struct awaitable
	{
		bool await_ready() const noexcept
		{
			return false;
		}

		void await_resume() const noexcept
		{
		}

		void await_suspend(std::experimental::coroutine_handle<> handle) const noexcept
		{
			PostMessageW(g_hWnd, WM_RESUMECORO, reinterpret_cast<WPARAM>(handle.address()), 0);
		}
	};

	return awaitable{};
}