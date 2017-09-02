#ifndef LEVIDB_INDEX_REGEX_ITER_H
#define LEVIDB_INDEX_REGEX_ITER_H

/*
 * 在已有基础上, 实现 iter 和 regex 的比特退化树
 *
 * iter 思路:
 * 在 valid 期间, index 不允许 applyPending, 那会破坏递归结构
 * 与普通 CritBit Tree 一样 iterate, 但最后要和 pending 做归并以符合 MVCC 要求
 *
 * regex 思路:
 * 构造一个 regex 解释器, 返回状态: OK, NO, POSSIBLE
 * 在 trie 分叉的地方检测, 如果 OK || POSSIBLE 说明可以继续下去
 *
 * 解释器的输入为"universal string representation"(USR), 按 bit 描述数据, 类型: 1 || 0 || UNKNOWN
 * 比如, 匹配"A"的解释器应该对
 * USR"1 0 0 0 0 0 UNKNOWN" => OK & POSSIBLE
 *    "1 1 1 0 0 0 0"       => NO
 *    "1 0 0"               => POSSIBLE
 *
 * 使用 URS 的原因是 CritBitTree 每次分歧只 reveal 1bit 的信息
 * return 时必须 OK(全匹配)
 */

#include "index_mvcc_rd.h"

namespace LeviDB {
    class IndexIter : public IndexRead {
    private:
        class BitDegradeTreeNodeIter;

        class BitDegradeTreeIterator;

        mutable std::atomic<int> operating_iters{0};

    public:
        IndexIter(const std::string & fname, SeqGenerator * seq_gen, RandomAccessFile * data_file)
                : IndexRead(fname, seq_gen, data_file) {}

        IndexIter(const std::string & fname, OffsetToEmpty empty, SeqGenerator * seq_gen, RandomAccessFile * data_file)
                : IndexRead(fname, empty, seq_gen, data_file) {}

        ~IndexIter() noexcept = default;

        DELETE_MOVE(IndexIter);
        DELETE_COPY(IndexIter);

    public:
        std::unique_ptr<Iterator<Slice, std::string>>
        makeIterator(std::unique_ptr<Snapshot> && snapshot = nullptr) const noexcept;

        void tryApplyPending();
    };

    class UsrJudge {
    public:
        UsrJudge() noexcept = default;
        DEFAULT_COPY(UsrJudge);
        DEFAULT_MOVE(UsrJudge);

    public:
        virtual ~UsrJudge() noexcept = default;

        virtual bool possible(const USR & input) const = 0;

        virtual bool match(const USR & input) const = 0;
    };
}

#endif //LEVIDB_INDEX_REGEX_ITER_H