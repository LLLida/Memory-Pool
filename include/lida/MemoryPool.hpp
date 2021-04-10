/*
Copyright 2021 Adil Mokhammad
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstdint>
#include <limits>
#ifndef NDEBUG
#  include <stdexcept>
#endif
#include <vector>

namespace lida
{
	namespace detail
	{
		template<std::size_t ObjSize>
		class MemoryChunk
		{
		private:
			uint8_t* data;
			uint8_t current;
			uint8_t count;

			static constexpr uint8_t max = std::numeric_limits<uint8_t>::max();

		public:
			MemoryChunk()
				: current(0), count(max)
			{
				data = new uint8_t[ObjSize * max];
				for (std::size_t i = 0; i < max; i++)
					data[i * ObjSize] = i + 1;
			}
			~MemoryChunk() noexcept
			{
				if (data) delete[] data;
			}
			MemoryChunk(const MemoryChunk&) = delete;
			MemoryChunk& operator=(const MemoryChunk&) = delete;
			MemoryChunk(MemoryChunk&& rhs) noexcept
				: data(rhs.data), current(rhs.current), count(rhs.count)
			{
				rhs.data = nullptr;
			}
			MemoryChunk& operator=(MemoryChunk&& rhs) noexcept 
			{ 
				this->~MemoryChunk();
				new(this) MemoryChunk(std::move(rhs));
				return *this;
			}

			void* allocate()
			{
#ifndef NDEBUG
				if (!has_space())
					throw std::runtime_error("MemoryChunk:: out of storage");
#endif
				void* ptr = data + current * ObjSize;
				current = data[current * ObjSize];
				count--;
				return ptr;
			}

			void deallocate(void* ptr)
			{
#ifndef NDEBUG
				if (!contains(ptr))
					throw std::runtime_error("MemoryChunk:: passed invalid pointer to deallocate");
#endif
				auto bptr = reinterpret_cast<uint8_t*>(ptr);
				*bptr = current;
				current = (bptr - data) / ObjSize;
				count++;
			}

			bool has_space() const noexcept
			{
				return count != 0;
			}

			bool contains(void* ptr) const noexcept
			{
				auto bptr = reinterpret_cast<uint8_t*>(ptr);
				return (bptr >= data) && (bptr < data + ObjSize * max);
			}

			bool is_free() const noexcept
			{
				return count == max;
			}
		};
	}

	/**
	 * @brief STL compatible allocator which allocates and deallocates
	 * single objects fast. This allocator shares memory between instances.
	 * @detail Use this allocator with containers that live short and
	 * allocate thousands of objects.
	 * The allocator is not thread safe(for performance reason) so in
	 * multithreaded program use groups if you have many containers which use
	 * the same allocator or mutexes otherwise.
	 * @tparam T allocating type.
	 * @tparam G allocator's group use different storages on same type.
	 * It can be useful when you more than one performance critical threads
	 * which allocate objects of the same type.
	 */
	template<typename T, std::size_t G = 0>
	class MemoryPool
	{
	private:
		static auto& get_chunks()
		{
			static std::vector<detail::MemoryChunk<sizeof(T)>> chunks;
			return chunks;
		}

	public:
		static constexpr std::size_t group = G;
		using value_type = T;
		template<typename U>
		struct rebind
		{
			using other = MemoryPool<U>;
		};

		MemoryPool() = default;
		template<typename U>
		MemoryPool(const MemoryPool<U>&) noexcept {}

		/**
		 * @brief Allocate an object from shared storage.
		 * @param size made for compability with std::allocator_traits.
		 * passing `size != 1` is undefined behaviour.
		 */
		[[nodiscard]]
		static T* allocate(std::size_t size)
		{
			auto& chunks = get_chunks();
#ifndef NDEBUG
			if (size != 1)
				throw std::runtime_error("MemoryPool is only able for allocating single objects");
#endif
			for (auto& chunk : chunks)
				if (chunk.has_space())
					return reinterpret_cast<T*>(chunk.allocate());
			chunks.emplace_back();
			return reinterpret_cast<T*>(chunks.back().allocate());
		}

		/**
		 * @brief Deallocate an object from shared storage.
		 * @param size made for compability with std::allocator_traits.
		 * passing `size != 1` is undefined behaviour.
		 */
		static void deallocate(T* ptr, [[maybe_unused]] std::size_t size)
		{
			auto& chunks = get_chunks();
			for (auto it = chunks.begin(); it != chunks.end(); ++it)
			{
				auto& chunk = *it;
				if (chunk.contains(ptr))
				{
					chunk.deallocate(ptr);
					if (chunk.is_free()) 
						chunks.erase(it);
					return;
				}
			}
		}

		/**
		 * @brief Preallocate memory.
		 * @param numElements minimal count of objects to preallocate.
		 */
		static void reserve(std::size_t numElements)
		{
			auto& chunks = get_chunks();
			constexpr auto max = std::numeric_limits<uint8_t>::max();
			std::size_t count = (numElements % max == 0) ?
				numElements / max : numElements / max + 1;
			if (count > chunks.size())
				chunks.resize(count);
		}
	};
}
