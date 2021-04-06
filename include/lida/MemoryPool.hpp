#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace lida
{
	namespace detail
	{
		class MemoryChunk
		{
		private:
			uint8_t* data;
			uint8_t current;
			uint8_t count;

			static constexpr uint8_t max = std::numeric_limits<uint8_t>::max();

		public:
			MemoryChunk(std::size_t objSize)
				: current(0), count(max)
			{
				data = new uint8_t[objSize * max];
				for (std::size_t i = 0; i <= max; i++)
					data[i * objSize] = i + 1;
			}
			~MemoryChunk() noexcept
			{
				delete[] data;
			}

			void* allocate(std::size_t objSize)
			{
#ifndef NDEBUG
				if (!has_space())
					throw std::runtime_error("MemoryChunk:: out of storage");
#endif
				void* ptr = data + current * objSize;
				current = data[current * objSize];
				count--;
				return ptr;
			}

			void deallocate(void* ptr, std::size_t objSize)
			{
#ifndef NDEBUG
				if (!contains(ptr, objSize))
					throw std::runtime_error("MemoryChunk:: passed invalid pointer to deallocate");
#endif
				auto bptr = reinterpret_cast<uint8_t*>(ptr);
				*bptr = current;
				current = (bptr - data) / objSize;
				count++;
			}

			bool has_space() const noexcept
			{
				return count != 0;
			}

			bool contains(void* ptr, std::size_t objSize) const noexcept
			{
				auto bptr = reinterpret_cast<uint8_t*>(ptr);
				return (bptr >= data) && (bptr < data + objSize * max);
			}

			bool is_free() const noexcept
			{
				return count == max;
			}
		};
	}

	template<typename T>
	class LocalMemoryPool
	{
	private:
		std::vector<detail::MemoryChunk> chunks;

	public:
		using value_type = T;
		template<typename U>
		struct rebind
		{
			using type = LocalMemoryPool<U>;
		};

		T* allocate(std::size_t size)
		{
#ifndef NDEBUG
			if (size != 1)
				throw std::runtime_error("MemoryPool is only able for single object allocations");
#endif
			for (auto& chunk : chunks)
				if (chunk.has_space())
					return reinterpret_cast<T*>(chunk.allocate(sizeof(T)));
			chunks.emplace_back(sizeof(T));
			return reinterpret_cast<T*>(chunks.back().allocate(sizeof(T)));
		}

		void deallocate(T* ptr, [[maybe_unused]] std::size_t size)
		{
			for (auto it = chunks.begin(); it != chunks.end(); ++it)
			{
				auto& chunk = *it;
				if (chunk.contains(ptr, sizeof(T)))
				{
					chunk.deallocate(ptr, sizeof(T));
					if (chunk.is_free()) 
						chunks.erase(it);
					return;
				}
			}
		}

		void reserve(std::size_t numElements)
		{
			constexpr auto max = std::numeric_limits<uint8_t>::max();
			chunks.reserve((numElements % max == 0) ?
						   numElements / max : numElements / max + 1);
		}
	};

	template<typename T>
	class GlobalMemoryPool
	{
	private:
		static auto& instance()
		{
			static LocalMemoryPool<T> singleton;
			return singleton;
		}

	public:
		using value_type = T;
		template<typename U>
		struct rebind
		{
			using type = GlobalMemoryPool<U>;
		};

		static T* allocate(std::size_t size)
		{
			return instance().allocate(size);
		}

		static void deallocate(T* ptr, std::size_t size)
		{
			instance().deallocate(ptr, size);
		}

		static void reserve(std::size_t numElements)
		{
			instance().reserve(numElements);		
		}
	};
}
