#pragma once

#include "ice/meta/schema.h"
#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>

namespace ice::commands {

/// Owned, ordered named scalar results. Copies are independent; moves and swaps never allocate.
/// Storage is created on insertion so an empty or moved-from output remains a usable empty value.
/// This indirection keeps response transfer allocation-free even when the native map's move
/// constructor allocates an empty sentinel. Prepare all fields before publishing host state.
class output {
	using storage = std::map<std::string, meta::value, std::less<>>;
	std::unique_ptr<storage> fields_;

	storage &writable() {
		if (!fields_)
			fields_ = std::make_unique<storage>();
		return *fields_;
	}

public:
	using key_type           = storage::key_type;
	using mapped_type        = storage::mapped_type;
	using value_type         = storage::value_type;
	using size_type          = storage::size_type;
	using iterator           = storage::iterator;
	using const_iterator     = storage::const_iterator;
	using node_type          = storage::node_type;
	using insert_return_type = storage::insert_return_type;

	output() = default;
	output(std::initializer_list<value_type> values) {
		if (values.size())
			fields_ = std::make_unique<storage>(values);
	}
	output(const output &other) {
		if (other.fields_)
			fields_ = std::make_unique<storage>(*other.fields_);
	}
	output &operator=(const output &other) {
		if (this != &other) {
			output copy(other);
			swap(copy);
		}
		return *this;
	}
	output(output &&)            = default;
	output &operator=(output &&) = default;

	bool           empty() const { return !fields_ || fields_->empty(); }
	size_type      size() const { return fields_ ? fields_->size() : 0; }
	iterator       begin() { return fields_ ? fields_->begin() : iterator{}; }
	iterator       end() { return fields_ ? fields_->end() : iterator{}; }
	const_iterator begin() const { return fields_ ? fields_->begin() : const_iterator{}; }
	const_iterator end() const { return fields_ ? fields_->end() : const_iterator{}; }
	const_iterator cbegin() const { return begin(); }
	const_iterator cend() const { return end(); }
	template <class K> iterator find(const K &key) {
		return fields_ ? fields_->find(key) : iterator{};
	}
	template <class K> const_iterator find(const K &key) const {
		return fields_ ? fields_->find(key) : const_iterator{};
	}
	template <class K> bool contains(const K &key) const { return find(key) != end(); }
	mapped_type            &at(const key_type &key) {
        if (!fields_)
            throw std::out_of_range("Command output field not found");
        return fields_->at(key);
	}
	const mapped_type &at(const key_type &key) const {
		if (!fields_)
			throw std::out_of_range("Command output field not found");
		return fields_->at(key);
	}
	mapped_type                  &operator[](const key_type &key) { return writable()[key]; }
	mapped_type                  &operator[](key_type &&key) { return writable()[std::move(key)]; }
	template <class... Args> auto emplace(Args &&...args) {
		return writable().emplace(std::forward<Args>(args)...);
	}
	template <class... Args> auto try_emplace(Args &&...args) {
		return writable().try_emplace(std::forward<Args>(args)...);
	}
	auto insert(value_type value) { return writable().insert(std::move(value)); }
	void insert(std::initializer_list<value_type> values) { insert(values.begin(), values.end()); }
	insert_return_type insert(node_type &&node) {
		if (node.empty())
			return {end(), false, std::move(node)};
		return writable().insert(std::move(node));
	}
	node_type extract(const_iterator position) { return fields_->extract(position); }
	node_type extract(const key_type &key) { return fields_ ? fields_->extract(key) : node_type{}; }
	template <class It> void insert(It first, It last) {
		if (first != last)
			writable().insert(first, last);
	}
	template <class... Args> auto insert_or_assign(Args &&...args) {
		return writable().insert_or_assign(std::forward<Args>(args)...);
	}
	size_type erase(const key_type &key) { return fields_ ? fields_->erase(key) : 0; }
	iterator  erase(const_iterator position) { return fields_->erase(position); }
	void      clear() {
        if (fields_)
            fields_->clear();
	}
	void merge(output &other) {
		if (this == &other || !other.fields_)
			return;
		if (!fields_)
			swap(other);
		else
			fields_->merge(*other.fields_);
	}
	void        merge(output &&other) { merge(other); }
	void        swap(output &other) noexcept { fields_.swap(other.fields_); }
	friend void swap(output &left, output &right) noexcept { left.swap(right); }
	friend bool operator==(const output &left, const output &right) {
		if (left.empty() || right.empty())
			return left.empty() && right.empty();
		return *left.fields_ == *right.fields_;
	}
};

} // namespace ice::commands
