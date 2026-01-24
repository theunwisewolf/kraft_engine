#pragma once

namespace kraft {
struct CSV
{
    String8* header;
    String8* cells;
    u32      num_rows = 0;
    u32      num_cols = 0;
};

String8 consume_line(String8 str)
{
    String8 line = {};
    line.ptr = str.ptr;
    i32 i = 0;
    while (i < str.count && str.ptr[i] != '\n')
    {
        i++;
    }

    line.count = i;
    return line;
}

String8 consume_csv_string(buffer csv_buffer, u64* position)
{
    buffer result = {};
    u64    current_position = *position;
    char*  ptr = (char*)csv_buffer.ptr + current_position;
    bool   found_end_quote = true;

    // "alpha """
    // Escaped string
    if (*ptr == '"')
    {
        // Consume '"'
        ptr++;
        current_position++;
        found_end_quote = false;
    }

    result.ptr = (u8*)ptr;
    while (current_position < csv_buffer.count)
    {
        if ((*ptr == ',' || *ptr == '\n' || *ptr == '\r') && found_end_quote)
            break;

        // A double-quote could mean two things
        // - This is an 'escape' quote and another double-quote is next
        // - This is the 'end-marker' double quote for the string
        if (*ptr == '"')
        {
            // In either case, we don't want to put this double-quote in the final string
            // So we consume it
            ptr++;
            current_position++;

            // Now we check if this is an end-marker quote or not
            bool is_end_marker = false == ((current_position + 1) < csv_buffer.count && *(ptr + 1) == '"');
            if (is_end_marker)
            {
                found_end_quote = true;

                // We don't want to push the final double-quote
                continue;
            }
        }

        ptr++;
        current_position++;
        result.count++;
    }

    // If the string was delimited by a comma, we want to consume it
    // if (*ptr == ',')
    // {
    //     ptr++;
    //     current_position++;
    // }

    *position = current_position;
    return result;
}

static CSV ParseCSV(ArenaAllocator* arena, String8 path)
{
    CSV       result = {};
    TempArena scratch = ScratchBegin(&arena, 1);

    using namespace fs;
    buffer file_buf = fs::ReadAllBytes(scratch.arena, path);
    if (file_buf.count == 0)
    {
        KFATAL("Failed to read %S", path);
    }

    u64 i = 0;
    u32 column_count = 0;
    // u32 row_count = 0;
    // u32 row_capacity = 0;

    // Read the header and figure out the column count
    while (i < file_buf.count)
    {
        if (file_buf.ptr[i] == '\r')
        {
            i++;
            break;
        }
        else if (file_buf.ptr[i] == '\n')
        {
            break;
        }

        // String8 column_data = consume_csv_string(file_buf, &i);
        column_count++;

        if (file_buf.ptr[i] == ',')
        {
            i++;
        }
    }

    result.num_cols = column_count;

    // Consume the last line-ending
    i++;

    u64 data_start = i;

    // Figure out the number of rows in the data
    while (i < file_buf.count)
    {
        if (file_buf.ptr[i] == '\n')
        {
            result.num_rows++;
        }

        i++;
    }

    // File doesn't end with an empty new-line
    if (file_buf.ptr[i - 1] != '\n')
    {
        result.num_rows++;
    }

    column_count = 0;

    // Move back to the beginning of the data
    i = data_start;

    // Reserve the space for cells
    result.cells = ArenaPushArray(arena, String8, result.num_rows * result.num_cols);
    u32 row = 1;
    while (i < file_buf.count)
    {
        u32 columns_scanned_this_row = 0;
        // KDEBUG("----");
        // Read columns in the row
        while (i < file_buf.count)
        {
            // Ignore any extra columns
            if (columns_scanned_this_row == result.num_cols)
            {
                KWARN("Row %d contains more columns than the header (Expected = %d)\nAdditional columns will be ignored", row, result.num_cols);
                while (i < file_buf.count && file_buf.ptr[i] != '\n')
                    i++;

                break;
            }

            String8 scratch_cell_data = consume_csv_string(file_buf, &i);
            result.cells[column_count] = ArenaPushString8Copy(arena, scratch_cell_data);

            // KDEBUG("%.*s", result.cells[column_count].count, result.cells[column_count].ptr);
            column_count++;
            columns_scanned_this_row++;

            if (file_buf.ptr[i] == ',')
            {
                i++;
            }
            else if (file_buf.ptr[i] == '\r')
            {
                i++;
                break;
            }
            else if (file_buf.ptr[i] == '\n')
            {
                break;
            }
        }
        // KDEBUG("----");

        KASSERT((column_count % result.num_cols) == 0);
        row++;

        // Consume the last last-ending
        i++;
    }

    ScratchEnd(scratch);
    return result;
}

static String8 CSVCell(CSV csv, u32 r, u32 c)
{
    return csv.cells[r * csv.num_cols + c];
}
}