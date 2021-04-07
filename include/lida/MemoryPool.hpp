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
				for (std::size_t i = 0; i <= max; i++)
					data[i * ObjSize] = i + 1;
			}
			~MemoryChunk() noexcept
			{
				if (data) delete[] data;
			}
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
	 * @tparam T allocating type.
	 */
	template<typename T>
	class MemoryPool
	{
	private:
		static auto& get_chunks()
		{
			static std::vector<detail::MemoryChunk<sizeof(T)>> chunks;
			return chunks;
		}

	public:
		using value_type = T;
		template<typename U>
		struct rebind
		{
			using type = MemoryPool<U>;
		};

		MemoryPool() = default;
		template<typename U>
		MemoryPool(const MemoryPool<U>&) noexcept {}

		/**
		 * @brief Allocate an object from shared storage.
		 * @param size made for compability with std::allocator_traits.
		 * passing `size != 1` is undefined behaviour.
			 */
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
		static void deallocate(T* ptr, std::size_t size)
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
			if (numElements > chunks.size())
			{
				constexpr auto max = std::numeric_limits<uint8_t>::max();
				chunks.resize((numElements % max == 0) ?
							  numElements / max : numElements / max + 1);
			}
		}
	};
}
