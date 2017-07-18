#include "index.h"
#include "util.h"
#include <cstdlib>

#ifndef __clang__
#include <algorithm> // GCC
#include <cstring> // GCC
#endif

namespace LeviDB {
    CritPtr::~CritPtr() noexcept {
        if (!isNull()) {
            if (isVal()) {
                delete[] asVal();
            } else {
                delete asNode();
            }
        }
    }

    int BDNode::size() const noexcept {
        if (full()) {
            return static_cast<int>(_ptrs.size());
        }
        int cnt = 0;
        for (const CritPtr & ptr:_ptrs) {
            if (!ptr.isNull()) {
                ++cnt;
            } else {
                break;
            }
        }
        return cnt;
    }

    auto BDNode::getDiffLess() const noexcept {
        return [&](const uint32_t & a, const uint32_t & b) {
            if (a < b) {
                return true;
            } else if (a == b) {
                return _masks[&a - _diffs.cbegin()] < _masks[&b - _diffs.cbegin()];
            } else {
                return false;
            }
        };
    }

    // todo: insert may cause memory leak silently
    void BitDegradeTree::insert(char * kv) noexcept {
        BDNode * cursor = _root;

        while (true) {
            auto pos = findBestMatch(cursor, kv);
            int idx;
            bool direct;
            int size;
            std::tie(idx, direct, size) = pos;

            CritPtr & ptr = cursor->_ptrs[idx + direct];
            if (ptr.isNull()) {
                ptr.setVal(kv);
                break;
            } else if (ptr.isNode()) {
                cursor = ptr.asNode();
            } else {
                if (strcmp(ptr.asVal(), kv) != 0) {
                    combatInsert(ptr.asVal(), kv);
                }
                break;
            }
        }
    }

    const char * BitDegradeTree::find(const char * k) const noexcept {
        BDNode * cursor = _root;

        while (true) {
            auto pos = findBestMatch(cursor, k);
            int idx;
            bool direct;
            int size;
            std::tie(idx, direct, size) = pos;

            const CritPtr & ptr = cursor->_ptrs[idx + direct];
            if (ptr.isNull()) {
                return nullptr;
            } else if (ptr.isNode()) {
                cursor = ptr.asNode();
            } else {
                return ptr.asVal();
            }
        }
    }

    std::tuple<int, bool, int>
    BitDegradeTree::findBestMatch(const BDNode * node, const char * k) const noexcept {
        const uint32_t * cbegin = node->_diffs.cbegin();
        const uint32_t * cend;

        int size = node->size();
        if (size <= 1) {
            return {0, false, size};
        } else {
            cend = &node->_diffs[size - 1];
        }

        auto cmp = node->getDiffLess();
        while (true) {
            const uint32_t * min_it = std::min_element(cbegin, cend, cmp);
            uint32_t diff_at = *min_it;

            // left or right
            uint8_t crit_byte = strlen(k) > diff_at ? char_be_uint8(k[diff_at]) : static_cast<uint8_t>(0);
            bool direct = (static_cast<uint8_t>(1) + (node->_masks[min_it - node->_diffs.cbegin()] | crit_byte)) >> 8;
            if (!direct) { // left
                cend = min_it;
            } else { // right
                cbegin = min_it + 1;
            }

            if (cbegin == cend) {
                return {static_cast<int>(min_it - node->_diffs.cbegin()), direct, size};
            }
        }
    }

    void BitDegradeTree::combatInsert(const char * opponent, char * kv) noexcept {
        uint32_t diff_at = 0;
        while (opponent[diff_at] == kv[diff_at]) {
            ++diff_at;
        }

        uint8_t mask = char_be_uint8(opponent[diff_at] ^ kv[diff_at]);
        mask |= mask >> 1;
        mask |= mask >> 2;
        mask |= mask >> 4;
        mask = (mask & ~(mask >> 1)) ^ static_cast<uint8_t>(UINT8_MAX);
        bool direct = (static_cast<uint8_t>(1) + (mask | kv[diff_at])) >> 8;

        BDNode * cursor = _root;
        while (true) {
            int replace_idx;
            bool replace_direct;
            int cursor_size = cursor->size();

            auto cmp = cursor->getDiffLess();
            const uint32_t * min_it = std::min_element(cursor->_diffs.cbegin(),
                                                       cursor->_diffs.cbegin() + cursor_size - 1,
                                                       cmp);
            if (*min_it > diff_at
                || (*min_it == diff_at && cursor->_masks[min_it - cursor->_diffs.cbegin()] > mask)) {
                if (!direct) {
                    replace_idx = 0;
                    replace_direct = false;
                } else {
                    replace_idx = cursor_size - 1 - 1;
                    replace_direct = true;
                }
            } else {
                if (cursor_size <= 1) {
                    replace_idx = 0;
                    replace_direct = false;
                } else {
                    const uint32_t * cbegin = cursor->_diffs.cbegin();
                    const uint32_t * cend = cursor->_diffs.cbegin() + cursor_size - 1;

                    while (true) {
                        uint32_t crit_diff_at = *min_it;
                        uint8_t crit_byte =
                                strlen(kv) > crit_diff_at ? char_be_uint8(kv[crit_diff_at]) : static_cast<uint8_t>(0);
                        bool crit_direct = (static_cast<uint8_t>(1) +
                                            (cursor->_masks[min_it - cursor->_diffs.cbegin()] | crit_byte)) >> 8;
                        if (!crit_direct) {
                            cend = min_it;
                        } else {
                            cbegin = min_it + 1;
                        }

                        if (cbegin == cend) {
                            replace_idx = static_cast<int>(min_it - cursor->_diffs.cbegin());
                            replace_direct = crit_direct;
                            break;
                        } else {
                            const uint32_t * next_it = std::min_element(cbegin, cend, cmp);
                            if (*next_it > diff_at
                                || (*next_it == diff_at && cursor->_masks[next_it - cursor->_diffs.cbegin()] > mask)) {
                                if (!direct) {
                                    replace_idx = static_cast<int>(cbegin - cursor->_diffs.cbegin());
                                    replace_direct = false;
                                } else {
                                    replace_idx = static_cast<int>(cend - cursor->_diffs.cbegin()) - 1;
                                    replace_direct = true;
                                }
                                break;
                            }
                            min_it = next_it;
                        }
                    }
                }
            }

            const CritPtr & ptr = cursor->_ptrs[replace_idx + replace_direct];
            if (cursor->_diffs[replace_idx] > diff_at
                || (cursor->_diffs[replace_idx] == diff_at && cursor->_masks[replace_idx] > mask)
                || ptr.isVal()) {
                if (cursor->full()) {
                    makeRoom(cursor);
                    continue;
                }
                nodeInsert(cursor, replace_idx, replace_direct, direct, diff_at, mask, kv, cursor_size);
                break;
            }

            cursor = ptr.asNode();
        }
    }

#define add_gap(arr, idx, size) memmove(&arr[(idx) + 1], &arr[(idx)], sizeof(arr[0]) * ((size) - (idx)))
#define del_gap(arr, idx, size) memmove(&arr[(idx)], &arr[(idx) + 1], sizeof(arr[0]) * ((size) - ((idx) + 1)))

    void BitDegradeTree::nodeInsert(BDNode * node, int replace_idx, bool replace_direct,
                                    bool direct, uint32_t diff_at, uint8_t mask, char * kv,
                                    int size) noexcept {
        assert(!node->full());
        int ptr_idx;
        if (size == 1) {
            assert(replace_idx == 0);
            ptr_idx = direct;
        } else {
            if (!direct) { // right usable
                if (!replace_direct) { // move left
                    ptr_idx = replace_idx;
                } else { // move right
                    ++replace_idx;
                    ptr_idx = replace_idx;
                }
            } else { // left usable
                if (!replace_direct) { // move left
                    ptr_idx = replace_idx + direct;
                } else { // move right
                    ++replace_idx;
                    ptr_idx = replace_idx + direct;
                }
            }
        }

        add_gap(node->_diffs, replace_idx, size - 1);
        add_gap(node->_masks, replace_idx, size - 1);
        add_gap(node->_ptrs, ptr_idx, size);

        node->_diffs[replace_idx] = diff_at;
        node->_masks[replace_idx] = mask;
        node->_ptrs[ptr_idx].setVal(kv);
    }

    void BitDegradeTree::makeRoom(BDNode * parent) noexcept {
        auto cmp = parent->getDiffLess();

        int rnd = static_cast<int>(rand() % parent->_ptrs.size());
        int i = rnd;
        do {
            const CritPtr & crit_ptr = parent->_ptrs[i];
            if (crit_ptr.isNode() && !crit_ptr.asNode()->full()) {
                BDNode * node = crit_ptr.asNode();
                // try left
                if (i != 0) {
                    int diff_idx = i - 1;
                    if (diff_idx - 1 < 0
                        || cmp(parent->_diffs[diff_idx - 1], parent->_diffs[diff_idx])) {
                        if (diff_idx + 1 > parent->_diffs.size() - 1
                            || cmp(parent->_diffs[diff_idx + 1], parent->_diffs[diff_idx])) {
                            makeRoomPush(parent, node, diff_idx, false);
                            break;
                        }
                    }
                }
                // try right
                if (i != parent->_ptrs.size() - 1) {
                    int diff_idx = i;
                    if (diff_idx - 1 < 0
                        || cmp(parent->_diffs[diff_idx - 1], parent->_diffs[diff_idx])) {
                        if (diff_idx + 1 > parent->_diffs.size() - 1
                            || cmp(parent->_diffs[diff_idx + 1], parent->_diffs[diff_idx])) {
                            makeRoomPush(parent, node, diff_idx, true);
                            break;
                        }
                    }
                }
            }

            if (++i == parent->_ptrs.size()) {
                i = 0;
            }
        } while (i != rnd);

        if (parent->full()) {
            const uint32_t * biggest = std::max_element(parent->_diffs.cbegin(), parent->_diffs.cend(), cmp);
            makeNewRoom(parent, static_cast<int>(biggest - parent->_diffs.cbegin()));
        }
    }

    void BitDegradeTree::makeRoomPush(BDNode * parent, BDNode * child, int idx, bool direct) noexcept {
        size_t parent_size = parent->_ptrs.size();
        int child_size = child->size();

        if (!direct) { // merge left to front
            add_gap(child->_diffs, 0, child_size - 1);
            add_gap(child->_masks, 0, child_size - 1);
            add_gap(child->_ptrs, 0, child_size);

            child->_diffs[0] = parent->_diffs[idx];
            child->_masks[0] = parent->_masks[idx];
            child->_ptrs[0] = parent->_ptrs[idx];

            del_gap(parent->_diffs, idx, parent_size - 1);
            del_gap(parent->_masks, idx, parent_size - 1);
            del_gap(parent->_ptrs, idx, parent_size);
            parent->_ptrs[parent_size - 1].setNull();
        } else { // merge right to back
            child->_diffs[child_size - 1] = parent->_diffs[idx];
            child->_masks[child_size - 1] = parent->_masks[idx];
            child->_ptrs[child_size] = parent->_ptrs[idx + 1];

            del_gap(parent->_diffs, idx, parent_size - 1);
            del_gap(parent->_masks, idx, parent_size - 1);
            del_gap(parent->_ptrs, idx + 1, parent_size);
            parent->_ptrs[parent_size - 1].setNull();
        }
    }

    void BitDegradeTree::makeNewRoom(BDNode * parent, int idx) noexcept {
        BDNode * new_node = new BDNode;
        new_node->_diffs[0] = parent->_diffs[idx];
        new_node->_masks[0] = parent->_masks[idx];
        new_node->_ptrs[0] = parent->_ptrs[idx];
        new_node->_ptrs[1] = parent->_ptrs[idx + 1];

        size_t size = parent->_ptrs.size();
        del_gap(parent->_diffs, idx, size - 1);
        del_gap(parent->_masks, idx, size - 1);
        del_gap(parent->_ptrs, idx + 1, size);
        parent->_ptrs[size - 1].setNull();
        parent->_ptrs[idx].setNode(new_node);
    }

    void BitDegradeTree::remove(const char * k) noexcept {
        BDNode * parent = nullptr;
        std::tuple<int, bool, int> parent_info;

        BDNode * cursor = _root;
        while (true) {
            auto pos = findBestMatch(cursor, k);
            int idx;
            bool direct;
            int size;
            std::tie(idx, direct, size) = pos;

            const CritPtr & ptr = cursor->_ptrs[idx + direct];
            if (ptr.isNull()) {
                break;
            } else if (ptr.isNode()) {
                parent = cursor;
                parent_info = pos;
                cursor = ptr.asNode();
            } else {
                if (strcmp(ptr.asVal(), k) == 0) {
                    nodeRemove(cursor, idx, direct, size);
                    if (parent != nullptr) {
                        tryMerge(parent, cursor,
                                 std::get<0>(parent_info), std::get<1>(parent_info), std::get<2>(parent_info),
                                 size - 1);
                    }
                }
                break;
            }
        }
    }

    void BitDegradeTree::nodeRemove(BDNode * node, int idx, bool direct, int size) noexcept {
        assert(size - 1 >= 0);
        delete[] node->_ptrs[idx + direct].asVal();
        if ((size - 1) - (idx + 1) > 0) {
            del_gap(node->_diffs, idx, size - 1);
            del_gap(node->_masks, idx, size - 1);
        }
        del_gap(node->_ptrs, idx + direct, size);
        node->_ptrs[size - 1].setNull();
    }

#define add_n_gap(arr, idx, size, n) memmove(&arr[(idx) + (n)], &arr[(idx)], sizeof(arr[0]) * ((size) - (idx)))
#define cpy_all(dst, idx, src, size) memcpy(&dst[(idx)], &src[0], sizeof(src[0]) * (size));

    void BitDegradeTree::tryMerge(BDNode * parent, BDNode * child,
                                  int idx, bool direct, int parent_size,
                                  int child_size) noexcept {
        if (child_size == 1) {
            parent->_ptrs[idx + direct] = child->_ptrs[0];
            child->_ptrs[0].setNull();
            delete child;
        } else {
            assert(child_size > 1);

            if (parent->_ptrs.size() - parent_size + 1 >= child_size) {
                if (!direct) { // left
                    add_n_gap(parent->_diffs, idx, parent_size - 1, child_size - 1);
                    add_n_gap(parent->_masks, idx, parent_size - 1, child_size - 1);
                    add_n_gap(parent->_ptrs, idx + 1, parent_size, child_size - 1);

                    cpy_all(parent->_diffs, idx, child->_diffs, child_size - 1);
                    cpy_all(parent->_masks, idx, child->_masks, child_size - 1);
                    cpy_all(parent->_ptrs, idx, child->_ptrs, child_size);
                } else { // right
                    add_n_gap(parent->_diffs, idx + 1, parent_size - 1, child_size - 1);
                    add_n_gap(parent->_masks, idx + 1, parent_size - 1, child_size - 1);
                    add_n_gap(parent->_ptrs, idx + 1 + 1, parent_size, child_size - 1);

                    cpy_all(parent->_diffs, idx + 1, child->_diffs, child_size - 1);
                    cpy_all(parent->_masks, idx + 1, child->_masks, child_size - 1);
                    cpy_all(parent->_ptrs, idx + 1, child->_ptrs, child_size);
                }

                memset(&child->_ptrs[child_size - child_size], 0, sizeof(child->_ptrs[0]) * child_size);
                delete child;
            }
        }
    }

    size_t BitDegradeTree::size(const BDNode * node) const noexcept {
        size_t cnt = 0;
        for (int i = 0; i < node->_ptrs.size(); ++i) {
            const CritPtr & ptr = node->_ptrs[i];
            if (ptr.isNull()) {
                break;
            } else if (ptr.isVal()) {
                ++cnt;
            } else {
                cnt += size(ptr.asNode());
            }
        }
        return cnt;
    }
}