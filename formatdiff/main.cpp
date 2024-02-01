#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <sstream>
#include <cstring>

using u64 = uint64_t;
using int64 = int64_t;

struct seqval {
    u64 i=0;
    seqval() = default;
    seqval(u64 i) : i(i) {}
    bool is_notfound() const { return i == UINT64_MAX; }
    bool is_invalid() const { return i == UINT64_MAX-1; }
    bool is_newline() const { return i == UINT64_MAX-2; }
    void set_notfound() { i = UINT64_MAX; }
    void set_invalid() { i = UINT64_MAX-1; }
};

using Sequence = std::vector<seqval>;

struct AsciiFormat {};
struct BinaryFormat {};
struct PackedIntFormat {};
struct ThemistoFormat {};

//in a sequence
const u64 max_numbers = 100'000'000;
static constexpr std::streamsize BUFFER_SIZE = 1024 * 1024 * 1024; // 1 GB

class ISequenceReader {
public:
    virtual ~ISequenceReader() = default;
    virtual void read(Sequence& s) = 0;
    virtual bool eof() const = 0;
    bool is_error=false;
    virtual void init() = 0;
    virtual void print_error() = 0;
    virtual void print_file_offset() = 0;
};


template <typename Format>
class SequenceReaderImpl : public ISequenceReader {
    public:
    std::ifstream file;
    std::string filename;
    bool is_eof = false;
    std::string error;
    void set_error(const std::string& msg) {
        // std::cerr << "- Error: "<< msg << std::endl; //!debug
        error = msg;
        is_error = true;
    }
    void append_error(const std::string& msg) {
        // std::cerr << "- Error: "<< msg << std::endl; //!debug
        error += msg;
        is_error = true;
    }
    void print_error() override {
        std::cerr << "- Error for SequenceReader with format " << typeid(Format).name() << " and file " << filename << " :\n" << error << std::endl; 
    }
    void print_file_offset() override {
        std::cerr << "- File ("<< filename <<") tellg: " << file.tellg() << ", file_pos: " << file_pos << std::endl;
    }

    SequenceReaderImpl(const std::string& path) : file(path, std::ios::binary), filename(path) {
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file " + path);
        }
    }
    u64 file_pos=0;
    char* buffer = new char[BUFFER_SIZE];
    char* current_pos = buffer;
    std::streamsize remaining = 0;
    //only EOF if couldn't read anything
    // bool safe_read(void* data, std::streamsize size) {
    //     file.read(reinterpret_cast<char*>(data), size);
    //     if (file.fail()) {
    //         if (file.gcount()==0) {
    //             is_eof = true;
    //         }
    //         return false;
    //     }
    //     return true;
    // }
    bool safe_read(void* data, std::streamsize size) {
        // If there is not enough data in the buffer, refill the buffer
        while (remaining < size) {
            // Move any remaining data to the start of the buffer
            std::memmove(buffer, current_pos, remaining);//!probably bad when safe_read large amount of data
            current_pos = buffer;
            // Read more data from the file into the buffer
            file.read(buffer + remaining, BUFFER_SIZE - remaining);
            std::streamsize read = file.gcount();
            remaining += read;
            // If we couldn't read any more data, set is_eof and return false
            if (remaining < size) {
                if (read == 0) {
                    is_eof = true;
                }
                return false;
            }
        }
        // Copy the data from the buffer to the data pointer
        std::memcpy(data, current_pos, size);
        current_pos += size;
        file_pos += size;
        remaining -= size;
        return true;
    }
    std::pair<std::string, std::string> skip_header() {
        u64 length1, length2;
        std::string header1, header2;
        // Read the lengths and headers using safe_read
        if (!safe_read(&length1, sizeof(u64))){
            set_error("Failed to read length1");
            return {"", ""};
        }
        if (length1 > 100000){
            set_error("Length1 is too large: " + std::to_string(length1));
            return {"", ""};
        }
        header1.resize(length1);
        if (!safe_read(&header1[0], length1)){
            set_error("Failed to read header1");
            return {"", ""};
        }
        if (!safe_read(&length2, sizeof(u64))){
            set_error("Failed to read length2");
            return {header1, ""};
        }
        if (length2 > 100000){
            set_error("Length2 is too large: " + std::to_string(length2));
            return {header1, ""};
        }
        header2.resize(length2);
        if (!safe_read(&header2[0], length2)){
            set_error("Failed to read header2");
            return {header1, ""};
        }
        return {header1, header2};
    }
    void init() override {
        if constexpr (std::is_same_v<Format, ThemistoFormat>) {
            // Do nothing for Themisto format
            return;
        } else {
            auto [formatStr, versionStr] = skip_header();
            if (formatStr != get_format_name() || versionStr != "v1.0") {
                set_error("Invalid header. Expected: " + get_format_name() + " v1.0. Got: " + formatStr + " " + versionStr);
            }
        }
    }
    std::string get_format_name() {
        if constexpr (std::is_same_v<Format, AsciiFormat>) {
            return "ascii";
        } else if constexpr (std::is_same_v<Format, BinaryFormat>) {
            return "binary";
        } else if constexpr (std::is_same_v<Format, PackedIntFormat>) {
            return "packedint";
        }
        return "";
    }
    // treats EOF as newline
    bool get_line(std::string& str, std::size_t n, char delim = '\n')
    {
        str.clear();
        char ch;
        std::size_t count = 0;
        while (count < n && safe_read(&ch, 1)) {
            if (ch == delim) {
                return true;  // Delimiter encountered, exit
            }
            str.push_back(ch);
            ++count;
        }
        if (count>=n) {
            set_error("Line too long");
            return false;
        }
        bool noread = is_eof && count==0;
        if (noread) is_eof = true;
        // Return false if end-of-file reached before any characters could be read
        return !noread;
    }

    bool eof() const override{
        return is_eof;
    }

    void read(Sequence& s) override {
        read_impl(Format(),s);
    }

    std::string line;
    void read_impl(AsciiFormat, Sequence& seq) {//assumes header skipped
        if (is_eof) return;
        if (!get_line(line, sizeof(u64)*max_numbers)) return;

        size_t pos = 0;
        seqval newval;
        while (pos < line.size()) {
            bool negative = false;
            if (line[pos] == '-'){
                negative = true;
                pos++;
            }
            auto old_pos = pos;
            int64 value = ((int64)parse_decimal(line, pos))*(negative ? -1 : 1);
            if (pos == old_pos){
                set_error("Failed to read number at position " + std::to_string(pos) + " in line: " + line);
                return;
            }
            if (value==-1){
                newval.set_notfound();
            } else if (value==-2){
                newval.set_invalid();
            } else if (value<0){
                set_error("Negative value in sequence: " + std::to_string(value));
                return;
            } else {
                newval.i = value;
            }
            seq.push_back(newval);
            pos++; //skip space
        }
        return;
    }

    void read_impl(BinaryFormat, Sequence& seq) {
        if (is_eof) return;
        seqval newval;
        u64 count = 0;
        for (;;) {
            if (!safe_read(&newval.i, sizeof(u64))){
                if (is_eof){
                    is_eof = count==0; //if nothing read, then EOF
                    return;
                }
                set_error("Failed to read uint64 at position " + std::to_string(count) + " in sequence");
                return;
            }
            if (newval.is_newline()) return;
            seq.push_back(newval);
            ++count;
            if (count > max_numbers){
                set_error("Too many numbers in sequence");
                return;
            }
        }
    }

    void read_impl(PackedIntFormat, Sequence& seq) {
        if (is_eof) return;
        char byte;
        u64 value;
        u64 count = 0;
        for (;;) {
            if (!safe_read(&byte, 1)){
                if (is_eof){
                    is_eof = count==0; //if nothing read, then EOF
                    return;
                }
                set_error("Failed to read byte");
                return;
            }
            if (byte == 0b01000000) {
                seqval not_found;
                not_found.set_notfound();
                seq.push_back(not_found);
            } else if (byte == 0b01000001) {
                seqval invalid;
                invalid.set_invalid();
                seq.push_back(invalid);
            } else if (byte == 0b01000010) {
                return; // Newline: end of the sequence
            } else {
                value = 0;
                int shift = 0;
                while (byte & 0x80) {  // If the high bit is set, it's part of the packed integer
                    value |= (u64)(byte & 0x7F) << shift;
                    if (!safe_read(&byte, 1)) {
                        set_error("Unexpected end of file during packed int reading, at position " + std::to_string(count) + " in sequence");
                        return;
                    }
                    shift += 7;
                    if (shift > 64){
                        set_error("Packed int too large");
                        return;
                    }
                }
                if (byte&0x40){
                    set_error("Unexpected high bit set in last byte of packed int");
                    return;
                }
                value |= (u64)byte << shift;
                seqval packed_int_val;
                packed_int_val.i = value;
                seq.push_back(packed_int_val);
            }
            ++count;
            if (count > max_numbers){
                set_error("Too many numbers in sequence");
                return;
            }
        }
    }
    inline u64 parse_decimal(const std::string& str, size_t& pos) {
        u64 result = 0;
        while (pos < str.size() && isdigit(str[pos])) {
            result = result * 10 + (str[pos] - '0');
            pos++;
        }
        return result;
    }
    u64 expected_index = 0;
    void read_impl(ThemistoFormat, Sequence& seq) {
        if (is_eof) return;
        if (!get_line(line, sizeof(u64)*max_numbers)) return;

        size_t pos = 0;
        u64 index = parse_decimal(line, pos);
        if (pos == 0 || (pos < line.size() && line[pos] != ' ')) {
            set_error("Failed to read sequence index: " + line);
            return;
        }
        pos++;  // skip the space

        if (index != expected_index) {
            set_error("Unexpected sequence index: " + std::to_string(index));
            return;
        }
        ++expected_index;

        int64 value = 0;
        int64 prev_num = -1;
        while (pos < line.size()) {
            value = parse_decimal(line, pos);
            if (value <= prev_num) {
                set_error("Sequence is not sorted: " + std::to_string(value) + " <= " + std::to_string(prev_num));
                return;
            }
            prev_num = value;
            seq.push_back((u64)value);
            pos++; //skip space
        }
    }
};

std::unique_ptr<ISequenceReader> createSequenceReader(const std::string& format, const std::string& filePath) {
    if (format == "ascii") {
        return std::make_unique<SequenceReaderImpl<AsciiFormat>>(filePath);
    } else if (format == "binary") {
        return std::make_unique<SequenceReaderImpl<BinaryFormat>>(filePath);
    } else if (format == "packedint") {
        return std::make_unique<SequenceReaderImpl<PackedIntFormat>>(filePath);
    } else if (format == "themisto") {
        return std::make_unique<SequenceReaderImpl<ThemistoFormat>>(filePath);
    } else {
        throw std::runtime_error("Unknown format: " + format);
    }
}

bool compareSequences(const Sequence& seq1, const Sequence& seq2, std::string& error) {
    if (seq1.size() != seq2.size()) {
        error = "Sequences have different lengths: " + std::to_string(seq1.size()) + " != " + std::to_string(seq2.size());
        return false;
    }
    for (size_t i = 0; i < seq1.size(); ++i) {
        if (seq1[i].i != seq2[i].i) {
            error = "Sequences differ at position " + std::to_string(i) + ": " + std::to_string(seq1[i].i) + " != " + std::to_string(seq2[i].i);
            return false;
        }
    }
    return true;
}

std::unordered_set<std::string> formats = {"ascii", "binary", "packedint", "themisto"};

int CompareFiles(const std::string& format1, const std::string& format2, const std::string& file1, const std::string& file2){
    if (formats.find(format1) == formats.end()) {
        std::cerr << "Invalid format: " << format1 << std::endl;
        return 1;
    }
    if (formats.find(format2) == formats.end()) {
        std::cerr << "Invalid format: " << format2 << std::endl;
        return 1;
    }
    
    try {
        auto reader1 = createSequenceReader(format1, file1);
        auto reader2 = createSequenceReader(format2, file2);
        reader1->init();
        reader2->init();
        Sequence seq1, seq2;
        for(;;){
            seq1.clear();
            seq2.clear();
            try{
                reader1->read(seq1);
                reader2->read(seq2);
            } catch (const std::runtime_error& e) {
                std::cerr << "- Runtime Error: " << e.what() << std::endl;
                reader1->print_file_offset();
                reader2->print_file_offset();
                return 1;
            }
            if (reader1->is_error) {
                reader1->print_error();
                reader1->print_file_offset();
                return 1;
            }
            if (reader2->is_error) {
                reader2->print_error();
                reader2->print_file_offset();
                return 1;
            }
            std::string error;
            if (!compareSequences(seq1, seq2, error)) {
                std::cerr << "- Sequences differ: " << error << std::endl;
                reader1->print_file_offset();
                reader2->print_file_offset();
                return 1;
            }
            if (reader1->eof() && reader2->eof()) {
                break;
            }
            if (reader1->eof() && !reader2->eof()) {
                std::cerr << "- File 1 ended before file 2" << std::endl;
                reader1->print_file_offset();
                reader2->print_file_offset();
                return 1;
            }
            if (!reader1->eof() && reader2->eof()) {
                std::cerr << "- File 2 ended before file 1" << std::endl;
                reader1->print_file_offset();
                reader2->print_file_offset();
                return 1;
            }
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "- Runtime Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}



int main(int argc, char* argv[]) {
    // if (argc != 5 && !(argc == 6 && std::string(argv[5]) == "--list")) {
    //     std::cerr << "Usage: " << argv[0] << " <format1> <format2> <file1> <file2> (--list)" << std::endl;
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <format1> <format2> <file1> <file2>" << std::endl;
        return 1;
    }

    std::string format1 = argv[1];
    std::string format2 = argv[2];
    std::string file1 = argv[3];
    std::string file2 = argv[4];
    return CompareFiles(format1, format2, file1, file2);
}