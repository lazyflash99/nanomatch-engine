#pragma once
#include <string_view>
#include <vector>
#include <charconv>

namespace nanomatch {

/**
 * @brief High-performance, zero-copy CSV parser using string_view.
 */
class CSVParser {
public:
    struct Row {
        char type;
        uint64_t order_id;
        char side;
        uint32_t quantity;
        int64_t price;
    };

    static bool parse_row(std::string_view line, Row& row) {
        if (line.empty() || line[0] == 't') return false; // Skip header or empty

        auto split = [](std::string_view s, char delim) {
            std::vector<std::string_view> tokens;
            size_t start = 0, end = 0;
            while ((end = s.find(delim, start)) != std::string_view::npos) {
                tokens.push_back(s.substr(start, end - start));
                start = end + 1;
            }
            tokens.push_back(s.substr(start));
            return tokens;
        };

        auto tokens = split(line, ',');
        if (tokens.size() < 5) return false;

        row.type = tokens[0][0];
        std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), row.order_id);
        row.side = tokens[2][0];
        std::from_chars(tokens[3].data(), tokens[3].data() + tokens[3].size(), row.quantity);
        std::from_chars(tokens[4].data(), tokens[4].data() + tokens[4].size(), row.price);

        return true;
    }
};

} // namespace nanomatch
