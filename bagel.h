// Copyright (C) 2026 Moshe Sulamy

///
///

#pragma once
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <bit>
#include <type_traits>

namespace bagel
{
	/**** Parameters ****/
	inline constexpr int	MaxComponents = 16;
	inline constexpr bool	DynamicBags = true;
	inline constexpr int	IdBagSize = 10;
	inline constexpr int	InitialEntities = 100;
	inline constexpr int	InitialPackedSize = 50;
	inline constexpr bool	CallbackOnDelete = false;
	/** end parameters **/

	using id_type = int;
	struct ent_type { id_type id; };
	using mask_type =
		std::conditional_t<MaxComponents<=8, std::uint_fast8_t,
		std::conditional_t<MaxComponents<=16, std::uint_fast16_t,
		std::conditional_t<MaxComponents<=32, std::uint_fast32_t,
			std::uint_fast64_t>>>;


	struct NoInstance {	NoInstance() = delete; };
	struct NoCopy {
		NoCopy() = default; // default constructor
		NoCopy(const NoCopy&) = delete;
		NoCopy& operator=(const NoCopy&) = delete;
	};

	template <class T, int N>
	class StaticBag
	{
	public:
		int size() const { return _size; }
		static void ensure(int) {}
		void push(const T& val) { _arr[_size++] = val; }
		T pop() { return _arr[--_size]; }

		T& operator[](int idx) { return _arr[idx]; }
		const T& operator[](int idx) const { return _arr[idx]; }
	private:
		T	_arr[N];
		int _size = 0;
	};
	template <class T, int N>
	class DynamicBag : NoCopy
	{
	public:
		int size() const { return _size; }
		void ensure(int new_capacity) {
			if (new_capacity > _capacity) {
				_capacity = std::max(_capacity*2, new_capacity);
				_arr = static_cast<T*>(
					realloc(_arr, sizeof(T)*_capacity));
			}
		}
		void push(const T& val) {
			if (_size == _capacity) {
				_capacity *= 2;
				_arr = static_cast<T*>(
					realloc(_arr, sizeof(T)*_capacity));
			}
			_arr[_size] = val;
			++_size;
		}
		T pop() {
			return _arr[--_size];
		}
		~DynamicBag() {
			free(_arr);
		}

		T& operator[](int idx) { return _arr[idx]; }
		const T& operator[](int idx) const { return _arr[idx]; }
	private:
		T*		_arr = static_cast<T*>(malloc(sizeof(T) * N));
		int		_size = 0;
		int		_capacity = N;
	};

	template <class T, int N>
	using Bag = std::conditional_t<DynamicBags, DynamicBag<T,N>, StaticBag<T,N>>;

	using DeleteFunc = void (*)(ent_type);
	template <class T> struct Register;

	template <class T>
	class SparseStorage final : public NoInstance
	{
	public:
		static void add(ent_type ent, const T& val) {
			_comps.ensure(ent.id+1);
			_comps[ent.id] = val;
		}
		static void del(ent_type) {}
		static T& get(ent_type ent) {
			return _comps[ent.id];
		}
	private:
		static inline Bag<T,InitialEntities> _comps;
		__attribute__((used)) static inline Register<T> _reg{nullptr};
	};
	template <class T>
	class TaggedStorage final : public NoInstance
	{
	public:
		static void add(ent_type, const T&) {}
		static void del(ent_type) {}
		static T& get(ent_type) = delete;
	private:
		__attribute__((used)) static inline Register<T> _reg{nullptr};
	};
	template <class T>
	class PackedStorage final : public NoInstance
	{
	public:
		static void add(const ent_type ent, const T& val) {
			_idToComp.ensure(ent.id+1);
			_idToComp[ent.id] = _comps.size();
			_comps.push(val);
			_compToId.push(ent.id);
		}
		static void del(const ent_type ent) {
			int idx = _idToComp[ent.id];
			const id_type last = _compToId.pop();

			_comps[idx] = _comps.pop();
			_compToId[idx] = last;
			_idToComp[last] = idx;
		}
		static T& get(const ent_type ent) {
			return _comps[_idToComp[ent.id]];
		}
	private:
		static inline Bag<T,InitialPackedSize> _comps;
		static inline Bag<int,InitialEntities> _idToComp;
		static inline Bag<id_type,InitialPackedSize> _compToId;
		__attribute__((used)) static inline Register<T> _reg{del};
	};
	template <class T>
	class StackStorage final : public NoInstance
	{
	public:
		static void add(const ent_type ent, const T& val) {
			_idToComp.ensure(ent.id+1);
			if (_freeIdx.size() > 0) {
				const int idx = _freeIdx.pop();
				_idToComp[ent.id] = idx;
				_comps[idx] = val;
			}
			else {
				_idToComp.ensure(ent.id+1);
				_idToComp[ent.id] = _comps.size();
				_comps.push(val);
			}
			//TODO: remember empty/full cells
		}
		static void del(const ent_type ent) {
			_freeIdx.push(_idToComp[ent.id]);
		}
		static T& get(const ent_type ent) {
			return _comps[_idToComp[ent.id]];
		}
	private:
		static inline Bag<T,InitialPackedSize> _comps;
		static inline Bag<int,InitialEntities> _idToComp;
		static inline Bag<id_type,IdBagSize> _freeIdx;
		__attribute__((used)) static inline Register<T> _reg{del};
	};

	template <class T>
	struct Storage final : NoInstance {
		using type = SparseStorage<T>;
	};

	class Mask final
	{
	public:
		using bit_type = mask_type;
		static constexpr bit_type bit(const int idx) { return 1<<idx; }

		void set(const bit_type b) { _mask |= b; }

		void clear(const bit_type b) { _mask &= ~b; }
		void clear() { _mask = 0; }

		bool test(const bit_type b) const { return _mask & b; }
		bool test(const Mask m) const { return (_mask & m._mask) == m._mask; }

		int ctz() const { return _mask ? std::countr_zero(_mask) : -1; }
	private:
		mask_type	_mask{0};
	};

	inline int compCounter = -1;
	template <class>
	struct Component final : NoInstance
	{
		static inline const int				Index = ++compCounter;
		static inline const Mask::bit_type	Bit = Mask::bit(Index);
	};

	/// Main class of ECS world
	/// @brief ECS world
	class World final : public NoInstance
	{
		static inline Bag<Mask,InitialEntities>		_masks;
		static inline Bag<id_type,IdBagSize>		_ids;
		static inline id_type _maxId = -1;
		static auto& _deleters() {
			static Bag<DeleteFunc,MaxComponents> _deleters;
			return _deleters;
		}
	public:
		/// Creates a new entity in the ECS world
		/// @return The new entity
		static ent_type createEntity() {
			if (_ids.size() > 0)
				return {_ids.pop()};
			_masks.push(Mask{});
			return {++_maxId};
		}
		/// Deletes the given entity from the ECS world
		/// @param ent The entity to delete
		static void deleteEntity(ent_type ent) {
			if constexpr (CallbackOnDelete) {
				Mask m = _masks[ent.id];
				int ctz;
				while ((ctz = m.ctz()) >= 0) {
					if (_deleters()[ctz] != nullptr)
						_deleters()[ctz](ent);
					m.clear(Mask::bit(ctz));
				}
			}
			_masks[ent.id].clear();
			_ids.push(ent.id);
		}

		static const Mask& mask(ent_type e) {
			return _masks[e.id];
		}
		template <class T>
		static T& getComponent(ent_type e) {
			return Storage<T>::type::get(e);
		}
		template <class T>
		static void addComponent(ent_type ent, const T& comp) {
			_masks[ent.id].set(Component<T>::Bit);
			Storage<T>::type::add(ent,comp);
		}
		template <class T>
		static void delComponent(ent_type ent) {
			_masks[ent.id].clear(Component<T>::Bit);
			Storage<T>::type::del(ent);
		}
		template <class T>
		static void registerDeleter(DeleteFunc func) {
			while (_deleters().size() < Component<T>::Index+1)
				_deleters().push(nullptr);
			_deleters()[Component<T>::Index] = func;
		}

		static id_type maxId() { return _maxId; }
	};

	template <class T> struct Register
	{
		explicit Register(const DeleteFunc func) {
			World::registerDeleter<T>(func);
		}
	};

	class MaskBuilder
	{
	public:
		template <class T>
		MaskBuilder& set() {
			m.set(Component<T>::Bit);
			return *this;
		}
		Mask build() const { return m; }
	private:
		Mask m;
	};

	class Entity
	{
	public:
		Entity(ent_type ent) : _ent(ent) {}
		ent_type entity() const { return _ent; }

		static Entity create() { return World::createEntity(); }
		void destroy() const { World::deleteEntity(_ent); }

		const Mask& mask() const { return World::mask(_ent); }

		template <class T> T& get() const {
			return World::getComponent<T>(_ent);
		}
		template <class T> void add(const T& val) const {
			World::addComponent<T>(_ent,val);
		}
		template <class T> void del() const {
			World::delComponent<T>(_ent);
		}

		template <class T, class ...Ts> void addAll(const T& val, const Ts&... vals) const {
			add(val);
			if constexpr (sizeof...(Ts)>0)
				addAll(vals...);
		}
		template <class T, class ...Ts> void delAll() const {
			del<T>();
			if constexpr (sizeof...(Ts)>0)
				del<Ts...>();
		}

		template <class T> bool has() const { return mask().test(Component<T>::Bit); }
		bool test(const Mask& m) const { return mask().test(m); }

		static Entity first() { return Entity{{0}}; }
		bool eof() const { return _ent.id > World::maxId(); }
		void next() { ++_ent.id; }
	private:
		ent_type _ent;
	};
}
