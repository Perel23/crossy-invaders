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
	inline constexpr int	MaxComponents = 64;
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
		/// Swap-remove: O(1) unordered remove by index.
		void remove(int idx) { _arr[idx] = _arr[--_size]; }

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
		static constexpr bit_type bit(const int idx) { return (bit_type)1<<idx; }

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
		// Function-local statics: index is assigned on first use, so Bit can never
		// observe a not-yet-initialized Index (TU dynamic-init order is unordered
		// for template static data members — real crashes on MinGW).
		static int Index() {
			static const int idx = ++compCounter;
			return idx;
		}
		static Mask::bit_type Bit() { return Mask::bit(Index()); }
	};

	// ─── EntityQuery ─────────────────────────────────────────────────────────────
	// Pre-filtered entity list for one component mask.
	// The World keeps these up-to-date automatically via addComponent / delComponent
	// / deleteEntity hooks, so systems iterate only their matching entities.
	static constexpr int MaxQueryEntities = 512;
	static constexpr int MaxQueries       = 64;
	struct EntityQuery {
		Mask                                 filter{};
		StaticBag<id_type, MaxQueryEntities> entities{};
		int                                  curr = 0;
	};

	// Per-frame performance counters — compile with -DBAGEL_PERF=0 to remove all overhead.
#ifndef BAGEL_PERF
#  define BAGEL_PERF 1
#endif
#if BAGEL_PERF
	inline int g_entity_checks     = 0;  // entity iterations via queries  (new method)
	inline int g_query_loop_starts = 0;  // number of query-loop starts    (× maxId = old-method cost)
#  define BAGEL_COUNT(x) (x)
#else
	inline constexpr int g_entity_checks     = 0;
	inline constexpr int g_query_loop_starts = 0;
#  define BAGEL_COUNT(x) ((void)0)
#endif

	/// Main class of ECS world
	/// @brief ECS world
	class World final : public NoInstance
	{
		static inline Bag<Mask,InitialEntities>		         _masks;
		static inline Bag<id_type,IdBagSize>		         _ids;
		static inline id_type                                _maxId = -1;
		static inline StaticBag<EntityQuery, MaxQueries>     _queries{};
		static auto& _deleters() {
			static Bag<DeleteFunc,MaxComponents> _deleters;
			return _deleters;
		}

		// ── Internal query-maintenance hooks ─────────────────────────────────────
		// Called after setting the bit in addComponent: add entity to queries it
		// newly satisfies.
		static void _onAdd(ent_type ent, Mask oldMask) {
			const Mask newMask = _masks[ent.id];
			for (int qi = 0; qi < _queries.size(); qi++)
				if (!oldMask.test(_queries[qi].filter) && newMask.test(_queries[qi].filter))
					_queries[qi].entities.push(ent.id);
		}
		// Called after clearing the bit in delComponent: remove entity from queries
		// it no longer satisfies.
		static void _onDel(ent_type ent, Mask oldMask) {
			const Mask newMask = _masks[ent.id];
			for (int qi = 0; qi < _queries.size(); qi++) {
				if (oldMask.test(_queries[qi].filter) && !newMask.test(_queries[qi].filter)) {
					EntityQuery& eq = _queries[qi];
					for (int i = 0; i < eq.entities.size(); i++)
						if (eq.entities[i] == ent.id) { eq.entities.remove(i); break; }
				}
			}
		}
		// Called before clearing the mask in deleteEntity: remove entity from every
		// query (handles ID reuse — new entity with same ID starts clean).
		// Called before clearing the mask — removes entity from every query.
		// If the deletion is of the entity currently at the iteration cursor we
		// decrement curr so that nextQ() re-visits that slot (which now holds the
		// entity swapped in from the end) and does not skip it.
		static void _onDelete(ent_type ent) {
			for (int qi = 0; qi < _queries.size(); qi++) {
				EntityQuery& eq = _queries[qi];
				for (int i = 0; i < eq.entities.size(); i++) {
					if (eq.entities[i] == ent.id) {
						eq.entities.remove(i);
						if (i == eq.curr) --eq.curr; // back up so nextQ covers swapped slot
						break;
					}
				}
			}
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
			_onDelete(ent);                    // remove from all queries before mask is cleared
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
			const Mask oldMask = _masks[ent.id];
			_masks[ent.id].set(Component<T>::Bit());
			Storage<T>::type::add(ent, comp);
			_onAdd(ent, oldMask);              // update queries
		}
		template <class T>
		static void delComponent(ent_type ent) {
			const Mask oldMask = _masks[ent.id];
			_masks[ent.id].clear(Component<T>::Bit());
			Storage<T>::type::del(ent);
			_onDel(ent, oldMask);              // update queries
		}
		template <class T>
		static void registerDeleter(DeleteFunc func) {
			while (_deleters().size() < Component<T>::Index()+1)
				_deleters().push(nullptr);
			_deleters()[Component<T>::Index()] = func;
		}

		static id_type maxId() { return _maxId; }

		// ── EntityQuery public API ────────────────────────────────────────────────
		/// Register a filtered entity list. Call once per system (static local).
		/// @return Query index — pass to Entity::firstQ / eofQ / nextQ.
		static int createQuery(Mask filter) {
			EntityQuery q;
			q.filter = filter;
			for (id_type id = 0; id <= _maxId; id++)
				if (_masks[id].test(filter)) q.entities.push(id);
			const int idx = _queries.size();
			_queries.push(q);
			return idx;
		}
		/// Remove any stale entities (mask no longer matches). Called automatically
		/// by Entity::firstQ — rarely needed to call manually.
		static void cleanQuery(int qi) {
			EntityQuery& eq = _queries[qi];
			int i = 0;
			while (i < eq.entities.size())
				if (_masks[eq.entities[i]].test(eq.filter)) ++i;
				else                                          eq.entities.remove(i);
		}
		static void    queryRewind  (int qi) { _queries[qi].curr = 0; }
		static bool    queryEof     (int qi) { return _queries[qi].curr >= _queries[qi].entities.size(); }
		static void    queryNext    (int qi) { ++_queries[qi].curr; }
		static id_type queryCurrent (int qi) { return _queries[qi].entities[_queries[qi].curr]; }
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
			m.set(Component<T>::Bit());
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

		template <class T> bool has() const { return mask().test(Component<T>::Bit()); }
		bool test(const Mask& m) const { return mask().test(m); }

		static Entity first() { return Entity{{0}}; }
		bool eof() const { return _ent.id > World::maxId(); }
		void next() { ++_ent.id; }

		// ── Query-based iteration (O(matched) instead of O(all)) ─────────────────
		/// Start iterating a query: cleans stale entries, rewinds, returns first entity.
		/// Usage: for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) { ... }
		static Entity firstQ(int qi) {
			World::cleanQuery(qi);
			World::queryRewind(qi);
			BAGEL_COUNT(++g_query_loop_starts);
			if (World::queryEof(qi)) return Entity{ent_type{World::maxId()+1}}; // empty sentinel
			BAGEL_COUNT(++g_entity_checks);
			return Entity{ent_type{World::queryCurrent(qi)}};
		}
		bool eofQ(int qi) const { return World::queryEof(qi); }
		void nextQ(int qi) {
			World::queryNext(qi);
			if (!World::queryEof(qi)) {
				BAGEL_COUNT(++g_entity_checks);
				_ent = ent_type{World::queryCurrent(qi)};
			}
		}
	private:
		ent_type _ent;
	};
}
