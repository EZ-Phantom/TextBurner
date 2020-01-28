#ifndef TEXTBURNER_H
#define TEXTBURNER_H

#include <map>
#include <locale>
#include <utility>
#include <codecvt>
#include <opencv2/opencv.hpp>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

namespace netline {
namespace module {

/// Сама текстовая зона, умеет рассчитывать перенос строк и скейлить высоту зоны, при передаче буля в функцию
class TextZone {
public:
    TextZone(const std::wstring & text, const cv::Rect & zone, FT_Face & face, int text_space = 0)
        : m_text(text),
          m_text_space(text_space),
          m_zone(zone),
          m_ft_face(face) {
    }

    void move(const cv::Point move_to) {
        m_zone.x = move_to.x;
        m_zone.y = move_to.y;
    }

    void shift(const int x, const int y) {
        m_zone.x += x;
        m_zone.y += y;
    }

    void scale(const double scale_factor) {
        m_zone.x = static_cast<int>(m_zone.x * scale_factor);
        m_zone.y = static_cast<int>(m_zone.y * scale_factor);
        m_zone.width = static_cast<int>(m_zone.width * scale_factor);
        m_zone.height = static_cast<int>(m_zone.height * scale_factor);
    }

    void resize(const int width, const int height) {
        m_zone.width = width;
        m_zone.height = height;
    }

    cv::Rect getZoneRect() const { return m_zone;}
    std::vector<std::wstring> getWTextRows() const { return m_text_rows;}

    /// Основная функция, нужна для создания переноса строк, когда необходимо, и подтягивания высоты самой зоны к тексту
    void createRowsFromText(const bool fit_text_zone_height_to_rows) {
        createTextRowsAndExpandHeightIfNeeded(fit_text_zone_height_to_rows);
    }

private:
    void createTextRowsAndExpandHeightIfNeeded(const bool fit_text_zone_height_to_rows) {
        std::vector<std::wstring> words;
        std::wstringstream wss(m_text);
        std::wstring word;
        while(std::getline(wss, word, L' ')) {
            word.insert(0, 1, L' ');
            words.push_back(word);
        }

        /// тут удаляются пробелы, при необходимости использования нескольких пробелов подряд - нужно будет усложнить тело while.
        /// (!)следует обратить внимание, что все слова, после первого, идут с префиксным пробелом(!)
        words.front().erase(0, 1);

        int symbol_width = static_cast<int>(calculateStringWidth(L"w"));
        int symbol_height = static_cast<int>(calculateStringHeight());

        /// добавляем строку и увеличиваем высоту зоны, если нужно
        m_text_rows.push_back(std::wstring());
        if (m_zone.height < symbol_height)
            resize(m_zone.width, symbol_height);

        const size_t max_symbols_in_row = static_cast<size_t>(m_zone.width / symbol_width);
        /// вот тут считаем по словам, переносим по словам, и если слово не помещается целое по ширине, то его будем переносить посимвольно.
        for (auto iter = words.begin(); iter != words.end(); iter++) {
            size_t word_length = iter->size();

            if (static_cast<size_t>(word_length + m_text_rows.back().size()) > max_symbols_in_row) {
                if (m_text_rows.back().empty()) {
                    // $TODO: вот тут самое интересное, в строке нет еще ни слова, но при этом уже первое не помещается, критический случай
                    // нужен перенос посимвольный.
                    if (iter->front() == L' ')
                        iter->erase(0,1);

                    m_text_rows.back().append(iter->substr(0, static_cast<size_t>(max_symbols_in_row)));
                    iter->erase(0, static_cast<size_t>(max_symbols_in_row));
                    m_text_rows.push_back(std::wstring());
                    iter--;
                } else {
                    /// отправляем на обработку то же самое слово, но уже на новой строке, ыхыхыхых.
                    m_text_rows.push_back(std::wstring());
                    /// обязательно удаляем предшествующий пробел, а то будет непонятный отступ в начале
                    iter->erase(0,1);
                    iter--;
                    if (m_zone.height < symbol_height * static_cast<int>(m_text_rows.size()))
                        resize(m_zone.width, symbol_height);
                }

            } else {
                m_text_rows.back().append(*iter);
            }
        }

        /// ресайзим зону по высоте, если изначальной недостаточно
        /// fit_text_zone_height_to_rows - флаг, который означает что высоту текстовой_зоны будем подгонять под ее содержимое
        const int needed_height = symbol_height * static_cast<int>(m_text_rows.size());
        if (m_zone.height < needed_height || fit_text_zone_height_to_rows)
            resize(m_zone.width, needed_height);

        resize(m_zone.width, m_zone.height + m_text_space);
    }

    uint calculateStringWidth(const std::wstring &string) const {
        const uint glyph_index = FT_Get_Char_Index(m_ft_face, static_cast<ulong>('w'));
        const uint string_size = static_cast<uint>(string.size());

        FT_GlyphSlot slot = m_ft_face->glyph;

        FT_Load_Glyph(m_ft_face, glyph_index, FT_LOAD_DEFAULT);
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

        /* FreeType 2 uses size objects to model all information related to a given character size for a given face.
         * For example, a size object holds the value of certain metrics like the ascender or text height,
         * expressed in 1/64th of a pixel, for a character size of 12 points (however, those values are rounded to integers, i.e., multiples of 64).
         */
        return static_cast<uint>(string_size * (slot->advance.x / 64));
    }

    uint calculateStringHeight() const {
        const uint glyph_index = FT_Get_Char_Index(m_ft_face, static_cast<ulong>('w'));
        FT_GlyphSlot slot = m_ft_face->glyph;

        FT_Load_Glyph(m_ft_face, glyph_index, FT_LOAD_DEFAULT );
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

        return static_cast<uint>(slot->metrics.vertAdvance / 64);
    }

private:
    std::wstring m_text;
    int m_text_space;
    cv::Rect m_zone;
    FT_Face & m_ft_face;
    std::vector<std::wstring> m_text_rows;
};

/******************************************************************/

class TextPositioner {
public:
    enum WorkModeFlag {
        REMOVE_EMPTY_SPACE_Y = 0x01,
        REMOVE_EMPTY_SPACE_X = 0x02,
        SCALE_Y = 0x04,
        SCALE_X = 0x08,
        NO_INTERSECTIONS = 0x10,
        TEXT_ZONE_HEIGHT_UP_TO_TEXT = 0x20,
        END_FLAG = 0x40
    };

public:
    TextPositioner(const int image_width)
        : m_image_width(image_width) {
        setOperateFlags(REMOVE_EMPTY_SPACE_Y | SCALE_Y | NO_INTERSECTIONS | TEXT_ZONE_HEIGHT_UP_TO_TEXT);
    }

    void setOperateFlags(const uint32_t flags) {
        /// коректная работа проверялась только с флагами:
        /// REMOVE_EMPTY_SPACE_Y | SCALE_Y | NO_INTERSECTIONS | TEXT_ZONE_HEIGHT_UP_TO_TEXT
        /// Остальные комбинации заложены на будущее и будет необходима сильная доработка ::placeCorrectlyTextZones()
        for (uint32_t i = 0; i < calculateWorkModeFlagPositionInEnum(END_FLAG); i++)
            m_work_mode.push_back(static_cast<bool>(flags & (1 << i)));
    }

    static uint calculateMonoSpaceFontSize(FT_Face & face,const int image_width) {
        const uint glyph_index = FT_Get_Char_Index(face, static_cast<ulong>('w'));
        const uint min_font_size = 12;
        const uint symbols_in_row = 80;

        uint font_size = 20;
        FT_Set_Pixel_Sizes(face, font_size, 0);
        FT_GlyphSlot slot = face->glyph;

        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

        /* FreeType 2 uses size objects to model all information related to a given character size for a given face.
         * For example, a size object holds the value of certain metrics like the ascender or text height,
         * expressed in 1/64th of a pixel, for a character size of 12 points (however, those values are rounded to integers, i.e., multiples of 64).
        */
        long symbol_width = slot->advance.x / 64;

        /// увеличиваем шрифт, пока длина строки, состоящая из symbols_in_row не превышает ширину изображения
        while (symbol_width * symbols_in_row < image_width) {
            font_size++;
            FT_Set_Pixel_Sizes(face, font_size, 0);
            FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
            FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
            symbol_width = slot->advance.x / 64;
        }

        /// уменьшаем шрифт, пока длина строки, состоящая из symbols_in_row не становится меньше ширины изображения
        while (symbol_width * symbols_in_row > image_width) {
            font_size--;
            FT_Set_Pixel_Sizes(face, font_size, 0);
            FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
            FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
            symbol_width = slot->advance.x / 64;
        }

        return font_size >= min_font_size ? font_size : min_font_size;
    }

    void placeCorrectlyTextZones(std::vector<TextZone> & text_zones) {
        //    setOperateFlags(REMOVE_EMPTY_SPACE_X | SCALE_Y | NO_INTERSECTIONS | TEXT_ZONE_HEIGHT_UP_TO_TEXT);
        const bool no_intersections = m_work_mode.at(calculateWorkModeFlagPositionInEnum(NO_INTERSECTIONS));
        const bool text_zone_up_to_height = m_work_mode.at(calculateWorkModeFlagPositionInEnum(TEXT_ZONE_HEIGHT_UP_TO_TEXT));
        const bool remove_empty_space_y = m_work_mode.at(calculateWorkModeFlagPositionInEnum(REMOVE_EMPTY_SPACE_Y));
        const bool scale_y = m_work_mode.at(calculateWorkModeFlagPositionInEnum(SCALE_Y));

        if (no_intersections) {
            removeIntersections(text_zones);
        }

        double scale_factor = 1.0;
        if (scale_y) {
            cv::Point left_top(INT_MAX, INT_MAX);
            cv::Point right_bottom(INT_MIN, INT_MIN);
            for (auto zone : text_zones) {
                cv::Rect rect = zone.getZoneRect();
                left_top.x = std::min(left_top.x, rect.x);
                left_top.y = std::min(left_top.y, rect.y);

                right_bottom.x = std::max(right_bottom.x, rect.x + rect.width);
                right_bottom.y = std::max(right_bottom.y, rect.y + rect.height);
            }
            scale_factor = static_cast<double>(m_image_width) / static_cast<double>(right_bottom.x - left_top.x);

            for (auto & zone : text_zones)
                zone.scale(scale_factor);
        }

        /// отрисовка и подгон текста, необходимый и обязательный цикл
        for (auto & text_zone : text_zones) {
            text_zone.createRowsFromText(text_zone_up_to_height);
        }

        if (text_zone_up_to_height) {
            if (no_intersections) {
                removeIntersections(text_zones);
            }
        }

        if (remove_empty_space_y) {
            std::map<int, size_t> text_zones_by_y = getSortedTextZones(text_zones);
            while (!text_zones_by_y.empty()) {
                const auto base_zone_iterator = text_zones_by_y.begin();
                TextZone & base_zone = text_zones.at(base_zone_iterator->second);
                const size_t base_zone_index = base_zone_iterator->second;
                const cv::Rect base_rect = base_zone.getZoneRect();
                const cv::Rect base_rect_extended(base_rect.x, 0, base_rect.width, INT_MAX - 1);

                int movable_distance = base_rect.y;
                for (size_t i = 0; i < text_zones.size(); i++) {
                    if (i == base_zone_index)
                        continue;

                    cv::Rect intersection = text_zones.at(i).getZoneRect() & base_rect_extended;
                    int current_distance = base_rect.y - intersection.y - intersection.height;
                    if (current_distance > 0)
                        movable_distance = std::min(movable_distance, current_distance);
                }

                base_zone.shift(0, -movable_distance);
                text_zones_by_y.erase(base_zone_iterator);
            }
        }
    }

private:
    void removeIntersections(std::vector<TextZone> & text_zones) {
        std::map<int, size_t> text_zones_by_y = getSortedTextZones(text_zones);
        while (!text_zones_by_y.empty()) {
            const auto base_zone_iterator = text_zones_by_y.begin();
            const size_t current_index = base_zone_iterator->second;
            const TextZone base_zone = text_zones.at(base_zone_iterator->second);
            const cv::Rect base_rect = base_zone.getZoneRect();
            for (size_t i = 0; i < text_zones.size(); i++) {
                if (i == current_index)
                    continue;

                cv::Rect current_rect = text_zones.at(i).getZoneRect();
                cv::Rect intersection = base_rect & current_rect;
                if (intersection.empty())
                    continue;

                /// все сравнения относительно первого. Двигаем только второй рект, первый считается установленым верно.
                /// положительный y_direction означает смещение второй зоны ВНИЗ
                /// положительный x_direction означает смещение второй зоны ВПРАВО
                /// используется система координат, нуль которой в верхнем левом углу монитора, ось oY направлена вниз, ось oX направлена направо.
                const int8_t x_direction = base_rect.x - current_rect.x > 0 ? -1 : 1;
                const int8_t y_direction = base_rect.x - current_rect.y > 0 ? -1 : 1;

                if (intersection.empty())
                    return;

                if (intersection.width >= (base_rect.width / 2.5)) {
                    /// используем смещение по игрику
                    if (y_direction > 0)
                        text_zones.at(i).move(cv::Point(current_rect.x, base_rect.y + 1));
                    else
                        text_zones.at(i).move(cv::Point(current_rect.x, current_rect.y - current_rect.height - 1));
                } else {
                    /// используем смещение по иксу
                    if (x_direction > 0)
                        text_zones.at(i).move(cv::Point(base_rect.x + base_rect.width + 1, current_rect.y));
                    else
                        text_zones.at(i).move(cv::Point(base_rect.x - current_rect.width - 1, current_rect.y));
                }
            }
            text_zones_by_y.erase(base_zone_iterator);
        }
    }

    std::map<int, size_t> getSortedTextZones(const std::vector<TextZone> & text_zones) {
        std::map<int, size_t> text_zones_by_y;
        for (size_t i = 0; i < text_zones.size(); i++)
            text_zones_by_y.insert(std::make_pair(text_zones.at(i).getZoneRect().y, i));

        return text_zones_by_y;
    }

    size_t calculateWorkModeFlagPositionInEnum(WorkModeFlag flag) {
        size_t counter = 0;
        while (flag > 0x01) {
            flag = static_cast<WorkModeFlag>(static_cast<size_t>(flag) >> 1);
            counter++;
        }
        return counter;
    }

private:
    const int m_image_width;
    std::vector<bool> m_work_mode;
};

/******************************************************************/

class TextBurnerException : public std::exception {
public:
    TextBurnerException(std::string msg) : m_msg(std::move(msg)) {}
    const char * what() const noexcept override {return (m_msg.c_str());}
private:
    std::string m_msg;
};

/******************************************************************/

/// Класс для отладки в отдельном проекте, НЕ использовать в torx, т.к. использует отрисовку и показ изображения
class TextBurnerDebuger {
public:
    /// Для отладки в отдельном проекте, НЕ использовать в torx, т.к. использует отрисовку и показ изображения
    static void showTextZonesFormation(const std::vector<TextZone> & text_zones) {
        cv::Point left_top(INT_MAX, INT_MAX);
        cv::Point right_bottom(INT_MIN, INT_MIN);
        for (auto zone : text_zones) {
            cv::Rect rect = zone.getZoneRect();
            left_top.x = std::min(left_top.x, rect.x);
            left_top.y = std::min(left_top.y, rect.y);

            right_bottom.x = std::max(right_bottom.x, rect.x + rect.width);
            right_bottom.y = std::max(right_bottom.y, rect.y + rect.height);
        }

        cv::Mat img(right_bottom.y - left_top.y + 1, right_bottom.x - left_top.x + 1, CV_8UC3, cv::Scalar(0, 0, 0));
        for (auto zone : text_zones) {
            cv::rectangle(img, zone.getZoneRect(), cv::Scalar(0xFF, 0xFF, 0xFF));
        }
        cv::imshow("test", img);
        cv::waitKey(0);
    }
};

class TextBurner {
public:
    TextBurner(const std::string & path_to_font) : m_image(nullptr),
        m_draw_frames(false),
        m_fit_text_zone_height_to_rows(false) {

      FT_Init_FreeType(&m_ft_library);
      FT_New_Face(m_ft_library, path_to_font.c_str(), 0, &m_ft_face);
      FT_Select_Charmap(m_ft_face, FT_ENCODING_UNICODE);
  }

    void setImage(cv::Mat * image) {
        m_image = image;
        FT_Set_Pixel_Sizes(m_ft_face, TextPositioner::calculateMonoSpaceFontSize(m_ft_face, m_image->cols), 0);
    }

    void appendTextZone(cv::Rect rect, const std::wstring & text) {
        m_text_zones.push_back(TextZone(text, rect, m_ft_face, 5));
    }

    void appendTextZone(cv::Rect rect, const std::string & text) {
        m_text_zones.push_back(TextZone(toWString(text), rect, m_ft_face, 5));
    }

    /// Каждый раз будет добавлять еще одну строку с текстом, крайне не рекомендуется использовать совместно с appendTextZone
    /// Либо у вас есть готовое расположение текстовок, либо вы пользуетесь этим методом.
    void appendTextRow(const std::wstring & text) {
        if (m_image == nullptr)
            throw TextBurnerException("set image before appending text!");

        cv::Rect rect(0, 0 + static_cast<int>(m_text_zones.size()) * 50,
                      m_image->cols, 50);
        m_text_zones.push_back(TextZone(text, rect, m_ft_face, 5));
    }

    void appendTextRow(const std::string & text) {
        if (m_image == nullptr)
            throw TextBurnerException("set image before appending text!");

        cv::Rect rect(0, 0 + static_cast<int>(m_text_zones.size()) * 50,
                      m_image->cols, 50);
        m_text_zones.push_back(TextZone(toWString(text), rect, m_ft_face, 5));
    }

    void setDrawTextZoneFrames(const bool draw_frames) { m_draw_frames = draw_frames;}

    void clearData() {
        m_image = nullptr;
        m_text_zones.clear();
    }

    void burnAllTextZones() {
        if (m_image == nullptr)
            return;

        TextPositioner text_positioner(m_image->cols);
        text_positioner.placeCorrectlyTextZones(m_text_zones);

        cv::Point left_top(INT_MAX, INT_MAX);
        cv::Point right_bottom(INT_MIN, INT_MIN);
        for (auto zone : m_text_zones) {
            cv::Rect rect = zone.getZoneRect();
            left_top.x = std::min(left_top.x, rect.x);
            left_top.y = std::min(left_top.y, rect.y);

            right_bottom.x = std::max(right_bottom.x, rect.x + rect.width);
            right_bottom.y = std::max(right_bottom.y, rect.y + rect.height);
        }

        const int image_original_height = m_image->rows;
        appendBackgroundToImage(cv::Scalar(0, 0, 0, 0), static_cast<int>(right_bottom.y - left_top.y));
        for (auto & text_zone : m_text_zones) {
            burnTextZoneToImage(text_zone, 0, image_original_height);

            if (m_draw_frames) {
                cv::Rect rect = text_zone.getZoneRect();
                rect.y += image_original_height;
                cv::rectangle(*m_image, rect, cv::Scalar(255, 255, 255));
            }
        }
    }
private:
    /// добавить зону для текстовых_зон
    void appendBackgroundToImage(const cv::Scalar & color, const int height) {
        m_image->push_back(cv::Mat(height, m_image->cols, m_image->type(), color));
    }

    /// отрисовать текстовую_зону
    void burnTextZoneToImage(const TextZone & text_zone, const int x_0, const int y_0) {
        FT_UInt previous = 0;
        long y_advance = 0;
        long posx = text_zone.getZoneRect().x;
        long posy = text_zone.getZoneRect().y;
        long x_advance = 0;
        std::vector<std::wstring> text_rows = text_zone.getWTextRows();
        int row_counter = 0;
        for (const std::wstring & row : text_rows) {
            row_counter++;
            for (size_t k = 0; k < row.length(); k++) {
                uint glyph_index = FT_Get_Char_Index(m_ft_face, static_cast<ulong>(row.c_str()[k]));
                FT_GlyphSlot slot = m_ft_face->glyph;  /* a small shortcut */

                /* FreeType 2 uses size objects to model all information related to a given character size for a given face.
                 * For example, a size object holds the value of certain metrics like the ascender or text height,
                 * expressed in 1/64th of a pixel, for a character size of 12 points (however, those values are rounded to integers, i.e., multiples of 64).
                */
                x_advance = slot->advance.x / 64; // считается только после Рендер_Глиф

                FT_Load_Glyph(m_ft_face, glyph_index, FT_LOAD_DEFAULT);
                FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
                y_advance = (slot->metrics.vertAdvance / 64) * row_counter;
                posx += x_advance;
                burnBitmapToImage(&slot->bitmap, static_cast<int>(posx + x_0 + slot->bitmap_left - x_advance), static_cast<int>(posy + y_0 + y_advance - slot->bitmap_top));
                previous = glyph_index;
            }
            posx = text_zone.getZoneRect().x;
        }
    }

    /// отрисовать один символ на m_image
    void burnBitmapToImage(FT_Bitmap * bitmap, const int x_shift, const int y_shift) {
        for (int row = 0; row < static_cast<int>(bitmap->rows); row++) {
            for (int col = 0; col < static_cast<int>(bitmap->width); col++) {
                unsigned char val = bitmap->buffer[col + (row * bitmap->pitch)];
                if (val != 0) {
                    m_image->at<cv::Vec3b>(row + y_shift, col + x_shift) = cv::Vec3b(val, val, val);
                }
            }
        }
    }

    std::wstring toWString(const std::string & string) {
        try {
            using convert_typeX = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_typeX, wchar_t> converterX;
            return converterX.from_bytes(string);
        } catch (std::out_of_range & exception) {
            throw TextBurnerException(std::string("Не удается конвертировать std::string <STR>" + string + "</STR> в std::wstring; ex.what()=" + exception.what()));
        }
        return L"";
    }

private:
    cv::Mat * m_image;
    std::vector<TextZone> m_text_zones;

    FT_Library m_ft_library;
    FT_Face m_ft_face; /* handle to face object */

    bool m_draw_frames;
    bool m_fit_text_zone_height_to_rows;
};

}
}

#endif // TEXTBURNER_H
