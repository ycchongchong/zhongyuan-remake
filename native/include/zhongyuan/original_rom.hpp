#pragma once
#include "campaign.hpp"
#include <array>
#include <cstddef>

namespace zhongyuan {
// Verified data and initialization for the selected Chinese ROM.
// Gameplay mutations are implemented natively in OriginalState.
class OriginalRom {
public:
    static constexpr std::uint32_t EXPECTED_CRC32 = 0xb0cf9573;
    static constexpr std::size_t FILE_SIZE = 393232;
    static constexpr std::size_t CITY_OFFSET = 0x271c;
    static constexpr std::size_t CITY_COUNT = 30;
    static constexpr std::size_t CITY_STRIDE = 36;
    static constexpr std::size_t OFFICER_OFFSET = 0x2b54;
    static constexpr std::size_t OFFICER_COUNT = 241;
    static constexpr std::size_t OFFICER_STRIDE = 8;
    static constexpr std::size_t VIRTUE_OFFSET = 0x32dc;
    static constexpr std::size_t CHR_OFFSET = 0x20010;
    static constexpr std::size_t TILE_COUNT = 16384;

    explicit OriginalRom(std::vector<std::uint8_t> bytes);
    Json inventory() const;
    Json world_map() const;
    Json battlefield(int city) const;
    Json deployment_tables(int city) const;
    int tactical_mobility(int units) const;
    std::array<int,2> tactical_strategy_offset(int index) const;
    std::array<int,2> tactical_nearby_offset(int index) const;
    Json tactical_strategy_tables() const;
    Json player_tactical_strategy_tables() const;
    int tactical_ai_step_cost(int officer_byte,int terrain) const;
    int tactical_occupied_fort_threshold(int index,bool attack) const;
    int tactical_step_cost(int officer_byte,int terrain) const;
    Json clash_terrain(int pair) const;
    int clash_position(int side,int formation,int unit) const;
    Json clash_map(int scene) const;
    std::vector<std::uint8_t> clash_rgb(int scene,int first_faction,int second_faction,const Json &units) const;
    int clash_direction(int index) const;
    int clash_retreat_threshold(int first_count,int second_count) const;
    int clash_desertion_threshold(int officer) const;
    int clash_desertion_size(int choice) const;
    int duel_ai_choice(int own_hp,int enemy_hp,int random,int frame) const;
    std::vector<std::uint8_t> battlefield_pixels(int city) const;
    std::vector<std::uint8_t> tactical_pixels(int city,const std::vector<std::uint8_t> &sram) const;
    Json development_tables() const;
    Json search_tables() const;
    Json ai_tables() const;
    Json command_tables() const;
    std::vector<std::uint8_t> initial_sram(int difficulty) const;
    int command_books(int difficulty, int city_count) const;
    Json city(std::size_t index) const;
    Json officer(std::size_t index) const;
    std::array<std::uint8_t,64> tile(std::size_t index) const;
    Json name_layout(bool is_officer, std::size_t index) const;
    std::array<std::uint8_t,48*16> name_pixels(bool is_officer, std::size_t index) const;
    static std::uint32_t crc32(const std::vector<std::uint8_t> &bytes);
private:
    std::vector<std::uint8_t> bytes_;
    std::uint32_t little(std::size_t offset, std::size_t width) const;
    Json raw(std::size_t offset, std::size_t length) const;
};
} // namespace zhongyuan
