#include "db_single.h"

namespace LeviDB {
    DBSingle::DBSingle(std::string name, Options options, SeqGenerator * seq_gen)
            : DB(std::move(name), options), _seq_gen(seq_gen) {
        std::string prefix = _name + '/' + _name;
        _file_lock.build(prefix + ".lock");

        if (IOEnv::fileExists(_name)) {
            if (_options.error_if_exists) {
                throw Exception::invalidArgumentException("DB already exists");
            }
            // 打开现有数据库
            std::string data_fname = prefix + ".data";
            if (!IOEnv::fileExists(data_fname)) {
                throw Exception::notFoundException("data file missing", data_fname);
            }
            std::string index_fname = prefix + ".index";
            std::string keeper_fname = prefix + ".keeper";
            if (!IOEnv::fileExists(index_fname) || !IOEnv::fileExists(keeper_fname)) {
                simpleRepair();
                return;
            }
            _meta.build(std::move(prefix));
            _af.build(data_fname);
            _rf.build(std::move(data_fname));
            _index.build(std::move(index_fname), _meta->immut_value()._offset, _seq_gen, _rf.get());
            _writer.build(_af.get());
        } else {
            if (!_options.create_if_missing) {
                throw Exception::notFoundException("DB not found");
            }
            // 新建数据库
            IOEnv::createDir(_name);
            _af.build(prefix + ".data");
            _rf.build(prefix + ".data");
            _index.build(prefix + ".index", _seq_gen, _rf.get());
            _writer.build(_af.get());
            _meta.build(std::move(prefix), DBSingleWeakMeta{}, ""); // WeakKeeper will add ".keeper" automatically
        }
    }

    void DBSingle::put(const WriteOptions & options,
                       const Slice & key,
                       const Slice & value) {
        RWLockWriteGuard write_guard(_rwlock);

        uint32_t pos = _writer->calcWritePos();
        std::vector<uint8_t> bin = LogWriter::makeRecord(key, value);
        _writer->addRecord({bin.data(), bin.size()});
        _index->insert(key, OffsetToData{pos});

        if (options.sync) _af->sync();
    };

    void DBSingle::remove(const WriteOptions & options,
                          const Slice & key) {
        RWLockWriteGuard write_guard(_rwlock);

        std::vector<uint8_t> bin = LogWriter::makeRecord(key, {});
        _writer->addDelRecord({bin.data(), bin.size()});
        _index->remove(key);

        if (options.sync) _af->sync();
    };

    void DBSingle::write(const WriteOptions & options,
                         const std::vector<std::pair<Slice, Slice>> & kvs) {
        RWLockWriteGuard write_guard(_rwlock);

        if (options.compress) {
            assert(options.uncompress_size != 0);
            uint32_t pos = _writer->calcWritePos();
            std::vector<uint8_t> bin = LogWriter::makeCompressRecord(kvs);
            if (bin.size() <= options.uncompress_size / 8 * 7) { // worth
                _writer->addCompressRecord({bin.data(), bin.size()});
                for (const auto & kv:kvs) {
                    _index->insert(kv.first, OffsetToData{pos});
                }

                if (options.sync) _af->sync();
                return;
            }
        }

        std::vector<std::vector<uint8_t>> group;
        group.reserve(kvs.size());
        std::transform(kvs.cbegin(), kvs.cend(), group.begin(), [](const std::pair<Slice, Slice> & kv) noexcept {
            return LogWriter::makeRecord(kv.first, kv.second);
        });

        std::vector<Slice> bkvs;
        bkvs.reserve(group.size());
        std::transform(group.cbegin(), group.cend(), bkvs.begin(),
                       [](const std::vector<uint8_t> & bkv) noexcept -> Slice {
                           return {bkv.data(), bkv.size()};
                       });

        std::vector<uint32_t> addrs = _writer->addRecords(bkvs);
        assert(kvs.size() == addrs.size());
        for (int i = 0; i < kvs.size(); ++i) {
            _index->insert(kvs[i].first, OffsetToData{addrs[i]});
        }

        if (options.sync) _af->sync();
    };

    std::pair<std::string, bool>
    DBSingle::get(const ReadOptions & options, const Slice & key) const {
        RWLockReadGuard read_guard(_rwlock);

        return _index->find(key, options.sequence_number);
    };

    std::unique_ptr<Snapshot>
    DBSingle::makeSnapshot() {
        RWLockWriteGuard write_guard(_rwlock);

        return _seq_gen->makeSnapshot();
    };

    std::unique_ptr<Iterator<Slice, std::string>>
    DBSingle::makeIterator(std::unique_ptr<Snapshot> && snapshot) const {
        RWLockReadGuard read_guard(_rwlock);

        return _index->makeIterator(std::move(snapshot));
    };

    std::unique_ptr<SimpleIterator<std::pair<Slice, std::string>>>
    DBSingle::makeRegexIterator(std::shared_ptr<Regex::R> regex,
                                std::unique_ptr<Snapshot> && snapshot) const {
        RWLockReadGuard read_guard(_rwlock);

        return _index->makeRegexIterator(std::move(regex), std::move(snapshot));
    };

    std::unique_ptr<SimpleIterator<std::pair<Slice, std::string>>>
    DBSingle::makeRegexReversedIterator(std::shared_ptr<Regex::R> regex,
                                        std::unique_ptr<Snapshot> && snapshot) const {
        RWLockReadGuard read_guard(_rwlock);

        return _index->makeRegexReversedIterator(std::move(regex), std::move(snapshot));
    };

    uint64_t DBSingle::indexFileSize() const noexcept {
        return _index->immut_dst().immut_length();
    };

    uint64_t DBSingle::dataFileSize() const noexcept {
        return _af->immut_length();
    };

    void DBSingle::explicitRemove(const WriteOptions & options, const Slice & key) {
        RWLockWriteGuard write_guard(_rwlock);

        uint32_t pos = _writer->calcWritePos();
        std::vector<uint8_t> bin = LogWriter::makeRecord(key, {});
        _writer->addDelRecord({bin.data(), bin.size()});
        _index->insert(key, OffsetToData{pos});

        if (options.sync) _af->sync();
    }

    void DBSingle::simpleRepair() noexcept {

    }

    Slice DBSingle::largestKey() const noexcept {

    };

    Slice DBSingle::smallestKey() const noexcept {

    };

    void DBSingle::updateKeyRange(const Slice & key) noexcept {

    };

    bool repairDBSingle(const std::string & db_single_name) noexcept {

    };
}
