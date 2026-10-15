#include "internal.h"
#include <atomic>

namespace ice::data {
namespace {
std::uint64_t allocate_owner() {
	static std::atomic<std::uint64_t> next{1};
	auto                              candidate = next.load(std::memory_order_relaxed);
	while (candidate != UINT64_MAX) {
		if (next.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed))
			return candidate;
	}
	throw std::overflow_error("Graph store identity space exhausted");
}
bool has_object(const graph_data &data, const object_reference &object) {
	auto found = data.objects.find({object.document, object.identity});
	if (found == data.objects.end())
		return false;
	const auto &record = detail::as<value::record>(found->second);
	return detail::text(detail::required(record, "$type"), 128) == object.type;
}
} // namespace
std::shared_ptr<const graph_data> prepared_graph::data() const { return data_; }
graph_store::graph_store(catalog definitions)
    : definitions_(std::make_shared<const catalog>(std::move(definitions))),
      owner_(allocate_owner()) {}
ice::expected<prepared_graph, diagnostic> graph_store::prepare(const std::string &document_uri,
                                                               const value       &document,
                                                               document_reader    reader) const {
	try {
		prepared_graph candidate;
		candidate.data_ = std::make_shared<const graph_data>(
		    detail::build_graph(*definitions_, document_uri, document, std::move(reader)));
		candidate.definitions_ = definitions_;
		return candidate;
	} catch (const detail::invalid &error) {
		return ice::unexpected(error.error);
	}
}
ice::expected<std::uint64_t, diagnostic> graph_store::activate(const prepared_graph &candidate,
                                                               std::uint64_t expected_revision) {
	auto revision = active_ ? active_->revision : 0;
	if (revision != expected_revision)
		return ice::unexpected(diagnostic{"Stale graph revision", {}});
	if (!candidate.data_ || candidate.definitions_ != definitions_)
		return ice::unexpected(
		    diagnostic{"Candidate belongs to a different store or was moved", {}});
	if (revision == UINT64_MAX)
		return ice::unexpected(diagnostic{"Graph revision/generation exhausted", {}});
	// Every allocation/copy happens before publication. Only noexcept swaps change active state.
	std::shared_ptr<const graph_snapshot> next = std::make_shared<const graph_snapshot>(
	    graph_snapshot{owner_, revision + 1, revision + 1, candidate.data_});
	auto history = history_;
	if (active_) {
		history.push_back(active_->data);
		if (history.size() > 8)
			history.erase(history.begin());
	}
	history_.swap(history);
	active_.swap(next);
	return active_->revision;
}
ice::expected<std::uint64_t, diagnostic> graph_store::undo(std::uint64_t expected_revision) {
	if (!active_ || active_->revision != expected_revision)
		return ice::unexpected(diagnostic{"Stale graph revision", {}});
	if (history_.empty())
		return ice::unexpected(diagnostic{"No graph state to undo", {}});
	if (active_->revision == UINT64_MAX)
		return ice::unexpected(diagnostic{"Graph revision/generation exhausted", {}});
	auto                                  revision = active_->revision + 1;
	std::shared_ptr<const graph_snapshot> next     = std::make_shared<const graph_snapshot>(
        graph_snapshot{owner_, revision, revision, history_.back()});
	history_.pop_back();
	active_.swap(next);
	return revision;
}
std::shared_ptr<const graph_snapshot>   graph_store::snapshot() const { return active_; }
ice::expected<graph_handle, diagnostic> graph_store::handle(const object_reference &object) const {
	if (!active_ || !has_object(*active_->data, object))
		return ice::unexpected(diagnostic{"Object is not active with the required type",
		                                  object.document + "#" + object.identity});
	return graph_handle{owner_, active_->generation, object};
}
ice::expected<value, diagnostic> graph_store::inspect(const graph_handle &handle) const {
	if (!active_ || handle.owner != owner_ || handle.generation != active_->generation)
		return ice::unexpected(diagnostic{"Stale or foreign graph handle", {}});
	if (!has_object(*active_->data, handle.object))
		return ice::unexpected(diagnostic{"Object/type is not active", handle.object.identity});
	return active_->data->objects.at({handle.object.document, handle.object.identity});
}
} // namespace ice::data
