#pragma once
#include "ice/meta/schema.h"
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace ice::meta {
/// Ordered, independently owned named scalar fields with nonallocating ownership moves.
/// Construct and populate before publishing effects; module-specific validators own budgets.
class scalar_fields {
	using storage = std::map<std::string, value, std::less<>>;
	std::unique_ptr<storage> fields_;
	storage                 &writable() {
        if (!fields_)
            fields_ = std::make_unique<storage>();
        return *fields_;
	}

public:
	/// Named field and owned scalar pair.
	using value_type = storage::value_type;
	/// Owned scalar-record bridge type; copies are ordinary fallible preparation.
	using map_type = storage;
	/// Owning extracted field node.
	using node_type = storage::node_type;
	/// Node insertion outcome, including unconsumed duplicate nodes.
	using insert_return_type = storage::insert_return_type;
	/// Mutable ordered field iterator.
	using iterator = storage::iterator;
	/// Immutable ordered field iterator.
	using const_iterator = storage::const_iterator;
	/// Create an empty result without allocation. Inserting fields may allocate.
	scalar_fields() noexcept = default;
	/// Prepare owned fields; allocation failure leaves the caller's state unchanged.
	scalar_fields(std::initializer_list<value_type> fields)
	    : fields_(std::make_unique<storage>(fields)) {}
	/// Copy fields independently; allocation failure propagates.
	scalar_fields(const scalar_fields &other)
	    : fields_(other.fields_ ? std::make_unique<storage>(*other.fields_)
	                            : std::make_unique<storage>()) {}
	/// Transfer ownership without allocation; the source becomes empty.
	scalar_fields(scalar_fields &&other) noexcept = default;
	/// Copy transactionally, preserving the destination on allocation failure.
	scalar_fields &operator=(const scalar_fields &other) {
		scalar_fields copy(other);
		swap(copy);
		return *this;
	}
	/// Transfer ownership without allocation; the source becomes empty.
	scalar_fields &operator=(scalar_fields &&other) noexcept = default;
	/// Replace fields transactionally from named scalar pairs.
	scalar_fields &operator=(std::initializer_list<value_type> fields) {
		scalar_fields copy(fields);
		swap(copy);
		return *this;
	}
	/// Compare ordered field names and scalar values without copying their storage.
	bool operator==(const scalar_fields &other) const {
		return size() == other.size() && std::equal(begin(), end(), other.begin(), other.end());
	}
	/// Number of owned fields.
	std::size_t size() const noexcept { return fields_ ? fields_->size() : 0; }
	/// Whether no fields are owned.
	bool empty() const noexcept { return size() == 0; }
	/// First mutable field, or end for an empty result.
	iterator begin() noexcept { return fields_ ? fields_->begin() : iterator{}; }
	/// Mutable end sentinel; empty and moved-from results are iterable.
	iterator end() noexcept { return fields_ ? fields_->end() : iterator{}; }
	/// First immutable field, or end for an empty result.
	const_iterator begin() const noexcept { return fields_ ? fields_->begin() : const_iterator{}; }
	/// Immutable end sentinel.
	const_iterator end() const noexcept { return fields_ ? fields_->end() : const_iterator{}; }
	/// Find a field without allocating a key.
	template <class Key> iterator find(const Key &key) {
		return fields_ ? fields_->find(key) : iterator{};
	}
	/// Find an immutable field without allocating a key.
	template <class Key> const_iterator find(const Key &key) const {
		return fields_ ? fields_->find(key) : const_iterator{};
	}
	/// Checked mutable access; a missing field throws out_of_range.
	value &at(const std::string &key) {
		if (!fields_)
			throw std::out_of_range("Missing scalar_fields field");
		return fields_->at(key);
	}
	/// Checked immutable access; a missing field throws out_of_range.
	const value &at(const std::string &key) const {
		if (!fields_)
			throw std::out_of_range("Missing scalar_fields field");
		return fields_->at(key);
	}
	/// Insert a default scalar if absent; allocation failure propagates.
	value &operator[](const std::string &key) { return writable()[key]; }
	/// Insert a default scalar using an owned key; allocation failure propagates.
	value &operator[](std::string &&key) { return writable()[std::move(key)]; }
	/// Construct a named field with ordinary map insertion semantics.
	template <class... Args> auto emplace(Args &&...args) {
		return writable().emplace(std::forward<Args>(args)...);
	}
	/// Copy a scalar record into independently owned fields.
	scalar_fields(const map_type &fields) : fields_(std::make_unique<storage>(fields)) {}
	/// Prepare fields from a scalar record; native map move may allocate on some hosts.
	scalar_fields(map_type &&fields) : fields_(std::make_unique<storage>(std::move(fields))) {}
	/// Copy the scalar record for an explicit, fallible interface bridge.
	map_type copy_fields() const { return fields_ ? *fields_ : map_type{}; }
	/// Insert a field only when absent, retaining associative duplicate semantics.
	template <class... Args> auto try_emplace(Args &&...args) {
		return writable().try_emplace(std::forward<Args>(args)...);
	}
	/// Insert or replace a named scalar; preparation may allocate or throw.
	template <class Key, class Mapped> auto insert_or_assign(Key &&key, Mapped &&mapped) {
		return writable().insert_or_assign(std::forward<Key>(key), std::forward<Mapped>(mapped));
	}
	/// Whether a named field exists, with transparent key lookup.
	template <class Key> bool contains(const Key &key) const { return find(key) != end(); }
	/// Insert a field range, retaining existing values for duplicate names.
	template <class Input> void insert(Input first, Input last) { writable().insert(first, last); }
	/// Insert a field list, retaining existing values for duplicate names.
	void insert(std::initializer_list<value_type> fields) { writable().insert(fields); }
	/// Remove a named field; missing fields leave the container unchanged.
	template <class Key> std::size_t erase(const Key &key) {
		auto found = find(key);
		if (found == end())
			return 0;
		fields_->erase(found);
		return 1;
	}
	/// Remove an existing mutable iterator and return its successor.
	iterator erase(iterator position) { return fields_->erase(position); }
	/// Remove an existing iterator and return its successor.
	iterator erase(const_iterator position) { return fields_->erase(position); }
	/// Extract an existing mutable iterator without copying its scalar value.
	node_type extract(iterator position) { return fields_->extract(position); }
	/// Extract an existing iterator without copying its scalar value.
	node_type extract(const_iterator position) { return fields_->extract(position); }
	/// Extract a named node; a missing field yields an empty node.
	template <class Key> node_type extract(const Key &key) {
		auto found = find(key);
		return found == end() ? node_type{} : fields_->extract(found);
	}
	/// Insert an owning node, returning duplicate ownership to the caller.
	insert_return_type insert(node_type &&node) { return writable().insert(std::move(node)); }
	/// Transfer nonduplicate nodes; duplicate keys remain in the source.
	void merge(scalar_fields &other) {
		if (!other.empty())
			writable().merge(*other.fields_);
	}
	/// Transfer nonduplicate nodes from a temporary field collection.
	void merge(scalar_fields &&other) { merge(other); }
	/// Filter fields explicitly; unlike std::erase_if, this accepts the owned wrapper.
	template <class Predicate> std::size_t erase_if(Predicate predicate) {
		if (!fields_)
			return 0;
		return std::erase_if(*fields_, std::move(predicate));
	}
	/// Remove all fields without allocating.
	void clear() noexcept {
		if (fields_)
			fields_->clear();
	}
	/// Exchange owned field storage without allocating or moving a native map.
	void swap(scalar_fields &other) noexcept { fields_.swap(other.fields_); }
};
} // namespace ice::meta
