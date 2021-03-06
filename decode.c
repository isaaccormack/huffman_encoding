#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_CHAR_ENCODING_LEN 64
#define MAX_NUM_TABLES 40
typedef struct Entry
{
    char c;
    uint8_t code_length_table_num;
} Entry;

void print_binary(uint64_t number, int start, int length)
{
    if (start < length)
    {
        print_binary(number >> 1, start + 1, length);
        putc((number & 1) ? '1' : '0', stdout);
    }
    if(start==0){
        printf("\n");
    }
}

int add_table_entry(char c, uint64_t binary, uint8_t bin_length, uint8_t table_n, Entry ***all_tables, int *total_number_of_tables)
{
    Entry *entry = (Entry *)malloc(sizeof(Entry));

    entry->c = c;
    entry->code_length_table_num = bin_length;

    Entry **table = all_tables[table_n];

    unsigned char lower = binary >> (MAX_CHAR_ENCODING_LEN - 8);
    unsigned char upper = lower | (0b11111111 >> bin_length);

    // printf("range(%d-%d)", lower, upper);
    register int i;
    for (i = lower; i <= upper; i++)
    {
        // If encoding length is <= 8, add entries
        if (bin_length <= 8)
        {
            if (table[i] == NULL || table[i]->code_length_table_num < entry->code_length_table_num)
            {
                table[i] = entry;
            }
        }
        // If encoding length is greater than 8, add entry in nested lookup table
        else
        {
            // printf("entr_inner ");
            if (table[i] == NULL)
            {
                // If there's no existing inner table, create one and fill.
                Entry **inner_table = (Entry **)malloc(256 * sizeof(Entry *));
                entry->code_length_table_num = *total_number_of_tables;
                all_tables[*total_number_of_tables] = inner_table;
                *total_number_of_tables = *total_number_of_tables + 1;
                add_table_entry(c, (binary << 8), bin_length - 8, entry->code_length_table_num, all_tables, total_number_of_tables);
                entry->c = 0;
                table[i] = entry;
            }
            else
            {
                // If there is an existing inner table, put entry in that table.
                add_table_entry(c, (binary << 8), bin_length - 8, table[i]->code_length_table_num, all_tables, total_number_of_tables);
            }
            // printf(" exit_inner ");
        }
    }
    return 0;
}

void build_lookup_table(FILE *input_file, Entry ***all_tables, int *total_number_of_tables){
    int c = fgetc(input_file);
    if (feof(input_file))
    {
        fprintf(stderr, "File is empty. Exiting...\n");
        exit(1);
    }
    Entry **root_table = all_tables[0];
    while (!feof(input_file))
    {
        uint8_t bin_length = 0;
        char binary_stream;

    
        // Read in one encoding mapping
        // This datatype must hold MAX_CHAR_ENCODING_LEN bits
        uint64_t binary = 0;
        while (1)
        {
            binary_stream = fgetc(input_file);
            if (binary_stream == '\n' || binary_stream == ' ')
            {
                break;
            }
            // putc(binary_stream, stdout);
            binary |= (uint64_t) (binary_stream == '1') << ((MAX_CHAR_ENCODING_LEN - 1) - bin_length);
            bin_length++;
        }

        // Add encoding mapping to table
        add_table_entry(c, binary, bin_length, 0, all_tables, total_number_of_tables);
        // printf(" %c(%d)\n", c, c);

        if (binary_stream == '\n')
        {
            break;
        }

        // Read next char
        c = fgetc(input_file);
    }
}

void decode_stream(Entry **root_table, FILE *input_file, Entry ***all_tables){
    register unsigned char file_buffer = 0;
    register int file_buffer_length = 0;
    register int neededbits = 8;
    register unsigned char buffer = 0;
    register uint8_t table_n = 0;
    Entry *entry;
    while (1)
    {
        // fill buffer from file
        buffer |= file_buffer >> (8 - neededbits);

        if (neededbits > file_buffer_length)
        {
            // If file buffer is too short, refill file buffer
            neededbits -= file_buffer_length;

            // Refresh file buffer
            file_buffer = fgetc(input_file);
            file_buffer_length = 8;

            // Read in remaining needed bits
            buffer |= file_buffer >> (8 - neededbits);
        }

        // Remove used bits from buffer
        file_buffer = file_buffer << (neededbits);
        file_buffer_length -= neededbits;

        // If there is no nested table, then print a character is associated with the encoding
        entry = all_tables[table_n][buffer];
        if (entry->c != 0)
        {
            if (entry->c == 26)
            {
                fprintf(stderr, "EOT\n");
                break;
            }
            // Get code length from lookup table entry
            neededbits = entry->code_length_table_num;
            // print the character associated with the encoding
            putc(entry->c,stdout);
            table_n = 0;
        }
        // If there is a nested table, then set the nested table and retrieve 8 more bits
        // This means that the associated encoding is longer than 8 bits and requires a secondary table
        else
        {
            table_n = entry->code_length_table_num;
            neededbits = 8;
        }

        // Erase used bits
        buffer = buffer << neededbits;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: ./decode <input_file.txt>\n");
        exit(1);
    }
    char *f_name = argv[1];
    FILE *input_file = fopen(f_name, "rb");
    if (input_file == NULL)
    {
        fprintf(stderr, "Can't open %s. Exiting...\n", f_name);
        exit(1);
    }

    Entry **all_tables[MAX_NUM_TABLES];
    int total_number_of_tables = 0;

    Entry *root_table[256] = {0};
    all_tables[total_number_of_tables] = root_table;
    total_number_of_tables++;
    build_lookup_table(input_file, all_tables, &total_number_of_tables);
    fprintf(stderr,"Number of tables: %d", total_number_of_tables);
    decode_stream(root_table, input_file, all_tables);
    fclose(input_file);
}