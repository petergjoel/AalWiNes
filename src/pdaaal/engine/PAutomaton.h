/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  Copyright Morten K. Schou
 */

/*
 * File:   PAutomaton.h
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 08-01-2020.
 */

#ifndef PDAAAL_PAUTOMATON_H
#define PDAAAL_PAUTOMATON_H

#include "pdaaal/model/PDA.h"

#include <memory>
#include <functional>
#include <vector>
#include <iostream>
#include <boost/functional/hash.hpp>


namespace pdaaal {

    struct trace_t {
        size_t _state = std::numeric_limits<size_t>::max(); // _state = p
        size_t _rule_id = std::numeric_limits<size_t>::max(); // size_t _to = pda.states()[_from]._rules[_rule_id]._to; // _to = q
        uint32_t _label = std::numeric_limits<uint32_t>::max(); // _label = \gamma
        // if is_pre_trace() {
        // then {use _rule_id (and potentially _state)}
        // else if is_post_epsilon_trace()
        // then {_state = q'; _rule_id invalid }
        // else {_state = p; _label = \gamma}

        trace_t() = default;

        trace_t(size_t rule_id, size_t temp_state)
                : _state(temp_state), _rule_id(rule_id), _label(std::numeric_limits<uint32_t>::max() - 1) {};

        trace_t(size_t from, size_t rule_id, uint32_t label)
                : _state(from), _rule_id(rule_id), _label(label) {};

        explicit trace_t(size_t epsilon_state)
                : _state(epsilon_state) {};

        [[nodiscard]] bool is_pre_trace() const {
            return _label == std::numeric_limits<uint32_t>::max() - 1;
        }
        [[nodiscard]] bool is_post_epsilon_trace() const {
            return _label == std::numeric_limits<uint32_t>::max();
        }
    };

    struct label_with_trace_t {
        uint32_t _label = std::numeric_limits<uint32_t>::max();
        const trace_t *_trace = nullptr;

        explicit label_with_trace_t(uint32_t label)
                : _label(label) {};

        explicit label_with_trace_t(const trace_t *trace) // epsilon edge
                : _trace(trace) {};

        label_with_trace_t(uint32_t label, const trace_t *trace)
                : _label(label), _trace(trace) {};

        bool operator<(const label_with_trace_t &other) const {
            return _label < other._label;
        }

        bool operator==(const label_with_trace_t &other) const {
            return _label == other._label;
        }

        bool operator!=(const label_with_trace_t &other) const {
            return !(*this == other);
        }

        [[nodiscard]] bool is_epsilon() const {
            return _label == std::numeric_limits<uint32_t>::max();
        }
    };

    class PAutomaton {
    private:
        struct temp_edge_t {
            size_t _from = std::numeric_limits<size_t>::max();
            size_t _to = std::numeric_limits<size_t>::max();
            uint32_t _label = std::numeric_limits<uint32_t>::max();

            temp_edge_t() = default;

            temp_edge_t(size_t from, uint32_t label, size_t to)
                    : _from(from), _to(to), _label(label) {};

            bool operator<(const temp_edge_t &other) const {
                if (_from != other._from) return _from < other._from;
                if (_label != other._label) return _label < other._label;
                return _to < other._to;
            }

            bool operator==(const temp_edge_t &other) const {
                return _from == other._from && _to == other._to && _label == other._label;
            }

            bool operator!=(const temp_edge_t &other) const {
                return !(*this == other);
            }
        };
        struct temp_edge_hasher {
            std::size_t operator()(const temp_edge_t& e) const
            {
                std::size_t seed = 0;
                boost::hash_combine(seed, e._from);
                boost::hash_combine(seed, e._to);
                boost::hash_combine(seed, e._label);
                return seed;
            }
        };

    public:
        struct state_t;

        struct edge_t {
            state_t *_to;
            std::vector<label_with_trace_t> _labels;

            // edge with a label and optional trace
            edge_t(state_t *to, uint32_t label, const trace_t *trace = nullptr)
                    : _to(to), _labels() {
                _labels.emplace_back(label, trace);
            };

            // epsilon edge with trace
            edge_t(state_t *to, const trace_t *trace) : _to(to), _labels() {
                _labels.emplace_back(trace);
            };

            // wildcard (all labels), no trace
            edge_t(state_t *to, size_t all_labels) : _to(to), _labels() {
                for (uint32_t label = 0; label < all_labels; label++) {
                    _labels.emplace_back(label);
                }
            };

            void add_label(uint32_t label, const trace_t *trace);

            bool contains(uint32_t label);

            [[nodiscard]] bool has_epsilon() const { return !_labels.empty() && _labels.back().is_epsilon(); }
            [[nodiscard]] bool has_non_epsilon() const { return !_labels.empty() && !_labels[0].is_epsilon(); }
        };

        struct state_t {
            bool _accepting = false;
            size_t _id;
            std::vector<edge_t> _edges;

            state_t(bool accepting, size_t id) : _accepting(accepting), _id(id) {};

            state_t(const state_t &other) = default;
        };

    public:
        // Accept one control state with given stack.
        PAutomaton(const PDA &pda, size_t initial_state, const std::vector<uint32_t> &initial_stack) : _pda(pda) {
            const size_t size = pda.states().size();
            const size_t accepting = initial_stack.empty() ? initial_state : size;
            for (size_t i = 0; i < size; ++i) {
                add_state(true, i == accepting);
            }
            auto last_state = initial_state;
            for (size_t i = 0; i < initial_stack.size(); ++i) {
                auto state = add_state(false, i == initial_stack.size() - 1);
                add_edge(last_state, state, initial_stack[i]);
                last_state = state;
            }
        };

        PAutomaton(PAutomaton &&) noexcept = default;

        PAutomaton(const PAutomaton &other) : _pda(other._pda) {
            std::unordered_map<state_t *, state_t *> indir;
            for (auto &s : other._states) {
                _states.emplace_back(std::make_unique<state_t>(*s));
                indir[s.get()] = _states.back().get();
            }
            // fix links
            for (auto &s : _states) {
                for (auto &e : s->_edges) {
                    e._to = indir[e._to];
                }
            }
            for (auto &s : other._accepting) {
                _accepting.push_back(indir[s]);
            }
            for (auto &s : other._initial) {
                _initial.push_back(indir[s]);
            }
        }

        void pre_star();

        void post_star();

        [[nodiscard]] const std::vector<std::unique_ptr<state_t>> &states() const { return _states; }
        
        [[nodiscard]] const PDA &pda() const { return _pda; }

        void to_dot(std::ostream &out,
                    const std::function<void(std::ostream &, const label_with_trace_t &)> &printer = [](auto &s,
                                                                                                        auto &e) {
                        s << e._label;
                    }) const;

        [[nodiscard]] bool accepts(size_t state, const std::vector<uint32_t> &stack) const;
        [[nodiscard]] std::vector<size_t> accept_path(size_t state, const std::vector<uint32_t> &stack) const;

        [[nodiscard]] const trace_t *get_trace_label(const std::tuple<size_t, uint32_t, size_t> &edge) const;
        [[nodiscard]] const trace_t *get_trace_label(size_t from, uint32_t label, size_t to) const;

        // TODO: Implement early termination versions.
        // bool _pre_star_accepts(size_t state, const std::vector<uint32_t> &stack);
        // bool _post_star_accepts(size_t state, const std::vector<uint32_t> &stack);

    protected:
        [[nodiscard]] size_t number_of_labels() const { return _pda.number_of_labels(); }

        size_t add_state(bool initial, bool accepting);
        size_t next_state_id() const;

        void add_epsilon_edge(size_t from, size_t to, const trace_t *trace);

        void add_edge(size_t from, size_t to, uint32_t label, const trace_t *trace = nullptr);

        void add_wildcard(size_t from, size_t to);

    private:
        const trace_t *new_pre_trace(size_t rule_id);
        const trace_t *new_pre_trace(size_t rule_id, size_t temp_state);
        const trace_t *new_post_trace(size_t from, size_t rule_id, uint32_t label);
        const trace_t *new_post_trace(size_t epsilon_state);

        std::vector<std::unique_ptr<state_t>> _states;
        std::vector<state_t *> _initial;
        std::vector<state_t *> _accepting;

        std::vector<std::unique_ptr<trace_t>> _trace_info;

        const PDA &_pda;
    };


}

#endif //PDAAAL_PAUTOMATON_H
