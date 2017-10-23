#ifdef LEVI_BENCH

#include <iostream>

#include "../src/index_mvcc_rd.h"
#include "../src/log_writer.h"
#include "source_fetcher.h"

void kv_del_bench() {
    if (LeviDB::IOEnv::fileExists(src_fname_)) {
        const std::string index_fname = "/tmp/levi_bench_index";
        const std::string data_fname = "/tmp/levi_bench_data";
        if (!LeviDB::IOEnv::fileExists(index_fname) || !LeviDB::IOEnv::fileExists(data_fname)) {
            return;
        }

        LeviDB::AppendableFile af(data_fname);
        LeviDB::RandomAccessFile rf(data_fname);

        LeviDB::SeqGenerator seq_g;
        LeviDB::IndexRead bdt(index_fname, LeviDB::OffsetToEmpty{LeviDB::IndexConst::disk_null_}, &seq_g, &rf);
        LeviDB::LogWriter writer(&af, af.immut_length());

        SourceFetcher src;
        for (int i = 0; i < test_times_; ++i) {
            auto item = src.readItem();
            uint32_t pos = writer.calcWritePos();
            std::vector<uint8_t> bin = LeviDB::LogWriter::makeRecord(item.first, item.second);
            writer.addRecord({bin.data(), bin.size()});
            bdt.remove(item.first, LeviDB::OffsetToData{pos});
        }

        std::cout << __FUNCTION__ << std::endl;
    }
}

#endif // LEVI_BENCH