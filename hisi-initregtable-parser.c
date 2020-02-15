/*
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *                    Version 2, December 2004
 *  
 * Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
 * 
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *  
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 * 
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 * 
 *
 *
 *
 * Hisi-initregtable-parser
 * janne kaikkonen (c) 2020
 *
 * Build: gcc -Wall -g hisi-initregtable-parser.c -o hisi-initregtable-parser
 * Usage: call the program without parameters to see usage with examples
 *
 * 
 * Parses HiSilicon SoC register tables(in binary format) used in bootloader(u-boot) with early low level function: init_registers(uint32_t* table_start_address, uint32_t mode)
 * Main purpose of this early low level function is to configure clocks and ddr phy/mem configuration in early stage so bootloader can be loaded in ddr ram.
 * Also io muxing is partially or completelly done at this phase.
 *
 * - Parsed init register table can be used to make eduncated guesses about undocumented hardware configuration! 
 *
 * Soc address space CSV file format(First Line omited)
 * BASE,     , END(OPT),   NAME
 * 0x00000000, 0xFFFFFFFF, REGISTER BASE
 *
 *
 * Init register table:
 * - start.S has area of zero fill after vector table usually with 64byte offset. Bootloader is built with  Binary format init register table is placed in this blank area using external tool or simple series of ld commands.
 *   - In start.S init vector table is padded to full 64bytes usually with 0x12345678(little endian). This can be used as signature to identify the start of init register table -> offset.
 *   - In start.S init register table is trailed with:
 *     - Pointer to start of init register table( used by caller of init_register() for pointer calculation)
 *     - Pointer to end of init register table
 *     - Padding 0xDEADBEEF(little endian) to 16bytes alignment. This can be used as signature to find end of init register table.
 * - Init register table is clearly identifiable in hex dump by repetitive patterns
 *   - Multiple accesses to similar addresses (ie. ddr phy register base)
 *   - Delay field mostly 0x00000000
 *   - Attribute field has a lot of similar attribute values ie. 0x000000fd(little endian)
 * - Valid init register table size must be multiple of 16bytes(size of one register table entry). Usually Register table is 4kB.
 * - Some firmwares may include multiple register tables for reasons as different hardware versions or development versions of tables have been left in binary.
 *
 *
 * Register table entry:
 * 32bits: Address Field - Address to Read or Write
 * 32bits: Value Field
 * 32bits: Delay Field
 * 32bits: Attributes Field
 *
 *
 * Iteration control:
 * - init_registers() will continue iterating table as long as (Address OR Value OR Delay OR Attributes).
 * - Null address doesn't terminate iterating!
 * - Valid register table entry should have one or more full null entry at end:
 *    0x00000000 0x00000000 0x00000000 0x00000000
 *
 *
 * "Number of bits" attribute(both read and write attribute):
 * - 0 equals 1bit
 * - 31 equals 32bits
 *
 *
 * "Start bit" attribute(both read and write attribute):
 * - 0 equals 0 shift
 * - 31 equals 31bit shift
 *
 * - If sum of "Number of bits" and "Start bit" exceeds 31 in read or write attributes table entry should be considered invalid.
 *
 *
 * Delay operation:
 * - Delay is a very basic loop 3 machine instruction "nop" loop counting down. Inspect init_registers() source to calculate cycle count if timing is critical.
 * - Actual delay varies based on set core clock that might change withing init_registers() process as clocks are altered by write operations to clock system registers.
 * - There is no compensation for core clock speed.
 * - Delay is performed after the register write or read operation has been done. Delay is also performed if no write or read operation is set.
 *
 *
 * Write Operation:
 * - Write operation is used to alter register value.
 * - Read more detailed description bellow.
 * - Write operation overrides potential read operation.
 *   - Table entry with both write and read flag should be considered invalid.
 *
 *
 * Read Operation:
 * - Read operation reads a register in a loop until it's shifted("start bit") and masked("number of bits"+1) value matches value field.
 * - Read operation is used for detecting lock bits(of PLLs etc.) and continue iterating register table after the lock has been archived.
 * - Note: Delay per  is performed even after read operation
 *
 *
 * Init_registers() iteration loop in "normal mode" (second parameter to init_registers() is 0x0):
 * - Start:
 * - If all fields are null:
 *   - Return(Only return point in the function)
 *
 * - Check If write flag:
 *   - Store value from register(pointed by address field)
 *   - Create new value by:
 *     - Masking(BitwiseAND) stored register value with inverted left shifted(by: "start bit") mask(width: "number of bits"+1).
 *     - Masking(BitwiseAND) (width: "number of bits"+1) value field.
 *     - Left shifting(by: "start bit") masked value field.
 *     - BitwiseOR masked&shifted value field with masked register value.
 *   - Write new value into register
 *
 * - Else If read flag:
 *   - Store value from register(pointed by address field)
 *   - Create comparison value by:
 *   - Right shift(by: "start bit") stored register value
 *   - Masking(BitwiseAND) shifted register value with mask(width: "number of bits"+1)
 *   - Compare value field to shifted&masked register value in a loop
 *     - Break the loop only when there is match
 *
 * - Perform delay if any
 * - Increment table pointer to next register table entry
 * - goto Start
 * 
 *
 * Init_registers() iteration loop in "pm mode" (second parameter to init_registers() is 0x1):
 * - More simple implementation that doesn't do bit shifting, but masking only.
 * - Write and read flags differ from "normal mode". Flag value 0x2 to read or write field performs particular operation.
 *   - This program will alert for invalid flag in case of 0x2.
 * - Not very usable and not used afaik.
 * - Not covered by this program.
 * - Use init_registers() as reference for exact/correct information about this mode if needed
 *
 *
 * Attributes in "normal mode":
 * 0-2LSB:(bitfield == 0x0) - no write
 *        (bitfield &  0x4) - write
 *        (bitfield &  0x2) - invalid(pm mode flag)
 * 
 * 3-7:   no of bits(=0-31) to be writen (See "Number of bits" section above)
 *
 * 8-10:  (bitfield == 0x0) - valid
 *        (bitfield != 0x0) - invalid
 *
 * 11-15: start bit(=0-31) to be writen
 *
 * 16-18: (bitfield == 0x0) - no read
 *        (bitfield &  0x4) - read
 *        (bitfield &  0x2) - invalid(pm mode flag)
 *
 * 19-23: no of bits(=0-31) to be read   (See "Number of bits" section above)
 *
 * 24-26: (bitfield == 0x0) - valid
 *        (bitfield != 0x0) - invalid
 *
 * 27-31MSB: start bit(=0-31) to be read
 *
 * Note on write and read flags: 0x5 widelly used(generated by hisilicon spreadsheet macro) instead of 0x4. It works the same as it has bit no. 2 set as does 0x4.
 *
 *
 */


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define SOC_REGISTER_NAME_LENGTH 15

typedef struct{
    uint32_t base_address;
    uint32_t end_address;
    char register_name[SOC_REGISTER_NAME_LENGTH];
} soc_register_type;


/* TODO add exclusion method(based on end address if set) and return -1 if no register matches */

int32_t get_register_index(uint32_t address, size_t number_of_registers, const soc_register_type *table){
    uint32_t temp;
    uint32_t closest_register_index = 0;
    uint32_t distance = UINT32_MAX;
    for(uint32_t i = 0; i<number_of_registers; i++){
        if(table[i].base_address <= address){               //Check that base address is smaller or equal to address 
            temp = (address - table[i].base_address);
            if(temp < distance){                            //Compare distances
                closest_register_index = i;                 //We have found the closest register base
                distance = temp;
            }
        }
    }
    return closest_register_index;                          //Return the register base index that was closest
}


/* SoC */

typedef struct{
    soc_register_type *soc_type_registers;
    size_t soc_type_registers_count;
    char *soc_type_parameter_str;
} soc_type;


/* "none" SoC. Do not remove */
const soc_register_type none_registers[] = {
    {
        0x0,
        UINT32_MAX,
        ""
    }
};

/* CSV import SoC */
soc_register_type *csv_soc_registers_ptr = NULL;


/* Soc List. Keep "none" and "csv" SoCs in their places in this list */
soc_type soc_list[] = {
    {
        (soc_register_type*)&none_registers,
        (sizeof(none_registers)/sizeof(soc_register_type)),
        "none"
    },
    {
        NULL,
        0,
        "csv"
    }
};


/* OPTIONAL PARAMETERS */

uint32_t color_enabled = 1;
uint32_t print_offset = 0;
uint32_t addresses_only = 0;
uint32_t no_address = 0;

#define ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_DETECTED_ERRORS_COUNT 1      //Just print error count
#define ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS 2               //Print errors

uint32_t attribute_validity_output_format = ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS;
uint32_t number_of_attribute_validity_errors_to_print = 1;
uint32_t print_how_many_attribute_validity_errors_omited = 1;


typedef struct{
    uint32_t *variable_to_alter_ptr;
    uint32_t variable_new_value;
    char *parameter_str;
} optional_parameter_type;

const optional_parameter_type optional_parameter_list[] = {
    {
        &color_enabled,
        0,
        "-nocolor"
    },
    {
        &addresses_only,
        1,
        "-addronly"
    },
    {
        &no_address,
        1,
        "-noaddress"
    },
    {
        &print_offset,
        1,
        "-printoffset"
    },
    {
        &attribute_validity_output_format,
        ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_DETECTED_ERRORS_COUNT,
        "-printerrorcount"
    },
    {
        &number_of_attribute_validity_errors_to_print,
        UINT32_MAX,
        "-printallerrors"
    }
};

int process_optional_parameters(int argc, char **argv, int argcoffset){                                             //argcoffset = how many parameters(including command name) to skip
    uint32_t i = 0;
    for(;argcoffset<argc;argcoffset++){                                                                             //Go through optional parameters
        for(i = 0; i < (sizeof(optional_parameter_list)/sizeof(optional_parameter_type)); i++){                     //For loop all optional parameters
            if(strcmp(argv[argcoffset],optional_parameter_list[i].parameter_str)==0){                               //If strings match
                *optional_parameter_list[i].variable_to_alter_ptr = optional_parameter_list[i].variable_new_value;  //Alter value of variable
                break;
            }
        }
        if(i==(sizeof(optional_parameter_list)/sizeof(optional_parameter_type))){                                   //We went through the whole list without break; - Unknown parameter
            return -1;  //Return error             
        }
    }
    return 0;       //Return success
}

/* STRINGS */

void change_stdout_green(){
    if(color_enabled){
        fprintf(stdout, "\x1B[32m");
    }
}
void change_stdout_red(){
    if(color_enabled){
        fprintf(stdout, "\x1B[31m");
    }
}
void change_stdout_yellow(){
    if(color_enabled){
        fprintf(stdout, "\x1B[33m");
    }
}
void change_stdout_blue(){
    if(color_enabled){
        fprintf(stdout, "\x1B[34m");
    }
}
void change_stdout_default(){
    if(color_enabled){
        fprintf(stdout, "\x1B[0m");
    }
}

/* LABEL STRINGS */

const char *addr_str =       "ADDR: ";
const char *reg_offset_str = "   OFFSET: ";
const char *value_str =      "   VALUE: ";
const char *delay_str =      "   DELAY: ";
const char *attr_str =       "   ATTR: ";


/* ATTRIBUTE OPERATION STRINGS */

const char *bit_count_str = " COUNT(0-31): ";
const char *bit_start_str = " START(0-31): ";

const char *write_4_str =    "  WRITE(0x4)  ";
const char *write_5_str =    "  WRITE(0x5)  ";
const char *inv_write_str =  "  WRITE(INVLD)";

const char *read_4_str =     "  READ(0x4)   ";
const char *read_5_str =     "  READ(0x5)   ";
const char *inv_read_str =   "  READ(INVLD) ";

const char *delay_only_str = "  DELAY ONLY  ";
const char *none_str =       "  NONE(INVLD) ";

const char *terminate_str =  "  (TERMINATE) ";


/* ATTRIBUTE ERROR STRINGS */

const char *null_addr_str =  "(NULL ADDR)";

const char *non_zero_attr_byte_range_8_10_str = "(NON-ZERO ATTR BYTE RANGE [8-10])";
const char *non_zero_attr_byte_range_24_26_str = "(NON-ZERO ATTR BYTE RANGE [24-26])";

const char *write_parameters_without_write_flag_str = "(WRITE PARAMETERS W/O WRITE FLAG)";
const char *read_parameters_without_read_flag_str = "(READ PARAMETERS W/O READ FLAG)";

const char *write_sum_of_count_and_start_exceeds_31_str = "(WRITE SUM OF BIT COUNT AND START BIT >31)";
const char *read_sum_of_count_and_start_exceeds_31_str = "(READ SUM OF BIT COUNT AND START BIT >31)";

const char *both_read_and_write_str = "(BOTH READ AND WRITE FLAGS ARE PRESENT)";


/* ERROR STRINGS */

#define ERROR_PARAMETER_COUNT               -1
#define ERROR_OPEN_FILE                     -2
#define ERROR_BYTES_OFFSET_PARAMETER        -3
#define ERROR_BYTES_COUNT_PARAMETER         -4
#define ERROR_RANGE_EXCEEDS_FILE            -5
#define ERROR_UNKNOWN_SOC_TYPE              -6
#define ERROR_UNKNOWN_OPTIONAL_PARAMETER    -7
#define ERROR_READ_FILE_ERROR               -8
#define ERROR_OPEN_CSV_FILE                 -9
#define ERROR_NO_LINES_CSV_FILE             -10
#define ERROR_CSV_MALLOC_FAILED             -11
#define ERROR_CSV_PARSING_ERROR             -12

void print_error_stderr(int error_no){
    if(error_no == ERROR_PARAMETER_COUNT){
        fprintf(stderr, "Usage: InputBinFile BytesOffset BytesCount SocType [OptionalParameters]\n");
        fprintf(stderr, "Example 1: ./hisi-initregtable-parser u-boot.bin 64 4k csv hi3516a_d.csv -printoffset\n");    
        fprintf(stderr, "Example 2: ./hisi-initregtable-parser u-boot.bin 64 4k csv hi3516_d.csv -nocolor > output.txt \n");
        fprintf(stderr, "Example 3: ./hisi-initregtable-parser u-boot.bin 64 4k none -addronly > addr_list.txt \n");
        fprintf(stderr, "SoC types:\n");
        for(uint32_t i = 0; i<(sizeof(soc_list)/sizeof(soc_type)); i++){
            fprintf(stderr, "%s\n", soc_list[i].soc_type_parameter_str);
        }
        fprintf(stderr, "Try optional parameters:\n");
        for(uint32_t i = 0; i<(sizeof(optional_parameter_list)/sizeof(optional_parameter_type)); i++){
            fprintf(stderr, "%s\n", optional_parameter_list[i].parameter_str);
        }  
    }
    else if(error_no == ERROR_OPEN_FILE){
        fprintf(stderr, "Open InputFile error!\n");
    }
    else if(error_no == ERROR_BYTES_OFFSET_PARAMETER){
        fprintf(stderr, "Check BytesOffset!\n");
    }
    else if(error_no == ERROR_BYTES_COUNT_PARAMETER){
        fprintf(stderr, "Check BytesCount! Must be !=0 and multiple of 16\n");
    }
    else if(error_no == ERROR_RANGE_EXCEEDS_FILE){
        fprintf(stderr, "Range exceeds file!\n");
    }
    else if(error_no == ERROR_UNKNOWN_SOC_TYPE){
        fprintf(stderr, "Unknown SoC type! Try:\n");
        for(uint32_t i = 0; i<(sizeof(soc_list)/sizeof(soc_type)); i++){
            fprintf(stderr, "%s\n", soc_list[i].soc_type_parameter_str);
        }
    }
    else if(error_no == ERROR_UNKNOWN_OPTIONAL_PARAMETER){
        fprintf(stderr, "Unknown optional parameter! Try:\n");
        for(uint32_t i = 0; i<(sizeof(optional_parameter_list)/sizeof(optional_parameter_type)); i++){
            fprintf(stderr, "%s\n", optional_parameter_list[i].parameter_str);
        }
        
    }
    else if(error_no == ERROR_READ_FILE_ERROR){
        fprintf(stderr, "Read InputFile error!\n");
    }
    else if(error_no == ERROR_OPEN_CSV_FILE){
        fprintf(stderr, "Open CSV InputFile error!\n");
    }
    else if(error_no == ERROR_NO_LINES_CSV_FILE){
        fprintf(stderr, "Not enough lines in CSV file!\n");
    }
    else if(error_no == ERROR_CSV_MALLOC_FAILED){
        fprintf(stderr, "malloc() for csv data failed!\n");
    }
    else if(error_no == ERROR_CSV_PARSING_ERROR){
        fprintf(stderr, "CVS parsing error line no: ");
    }
    return;
}


int import_csv_soc_registers(char *filename){
#define LENGTH_LINE 100
#define OMIT_LINES_COUNT 1
    const char delimiter[] = ",";       //Delimiter
    char line[LENGTH_LINE];             //Line
    char* line_ptr = line;              //Pointer
    uint32_t line_length = 0;
    uint32_t number_of_lines = 0;       //Number of lines in a file
    uint32_t line_number = 0;           //Counter for line number
    
    FILE *fptr = NULL;                  //File pointer
    
    fptr = fopen(filename ,"r");        //Open file
    
    uint32_t j = 0;

    /* Check that we have a file open */
    if(fptr == NULL){
        print_error_stderr(ERROR_OPEN_CSV_FILE);       //Return open input file error to stderr
        return ERROR_OPEN_CSV_FILE;
    }
        
    /* How many lines file has */
    while(!feof(fptr)){
        fgets(line,LENGTH_LINE,fptr);
        line_length = strlen(line);
        if(line[line_length-1] == '\n' || feof(fptr)){  //Newline or EOF
            number_of_lines++;                          //Increment line number
        }
        
        /* See that line has actual contents */
        line_ptr = line;                //Reset line pointer
        /* Field 0 - Base address   */
        if(number_of_lines>OMIT_LINES_COUNT){
            while(*line_ptr==' '){ 
                line_ptr++;
            }
            if((*line_ptr<48)||(*line_ptr>57)){
                fclose(fptr);                                   
                print_error_stderr(ERROR_CSV_PARSING_ERROR);
                fprintf(stderr, "%lu\n", number_of_lines);
                return ERROR_CSV_PARSING_ERROR;
            }
        }
        
    }
    
    rewind(fptr);                                       //Rewind file to 0
    
    if((number_of_lines < 2)){                          //Check we have lines
        fclose(fptr);                                   
        print_error_stderr(ERROR_NO_LINES_CSV_FILE);
        return ERROR_NO_LINES_CSV_FILE;
    }
    
    /* Malloc */
    csv_soc_registers_ptr = malloc(sizeof(soc_register_type)*(number_of_lines-OMIT_LINES_COUNT));
    if(csv_soc_registers_ptr==NULL){
        fclose(fptr);
        print_error_stderr(ERROR_CSV_MALLOC_FAILED);
        return ERROR_CSV_MALLOC_FAILED;
    }
    
    /* Read lines loop */
    for(uint32_t i = 0; ((i<OMIT_LINES_COUNT)&&(!feof(fptr))); i++){
        fgets(line,LENGTH_LINE,fptr);   //Omit lines
    }
    for(line_number = 0; ((line_number<(number_of_lines-OMIT_LINES_COUNT))&&(!feof(fptr)));line_number++){
        fgets(line,LENGTH_LINE,fptr);   //Get line
        line_ptr = line;                //Reset line pointer
        /* Field 0 - Base address   */
        while(*line_ptr==' '){ 
            line_ptr++;
        }
        if((*line_ptr=='\0')||(*line_ptr=='\n')){
            break;          //Break the loop
        }
        csv_soc_registers_ptr[line_number].base_address = strtoul(line_ptr, NULL, 0);   //Store base address    //TODO error handling
        while(*line_ptr!=*delimiter){
            line_ptr++;
            if((*line_ptr=='\0')||(*line_ptr=='\n')){
                break;          //Break the loop
            }
        }
        line_ptr++;     //Increment from delimiter
        
        
        
        /* Field 1 - End address    */
        while(*line_ptr==' '){  
            line_ptr++;
        }
        if((*line_ptr=='\0')||(*line_ptr=='\n')){
            break;          //Break the loop
        }
        csv_soc_registers_ptr[line_number].end_address = strtoul(line_ptr, NULL, 0);    //Store end address     //TODO error handling
        while(*line_ptr!=*delimiter){
            line_ptr++;
            if((*line_ptr=='\0')||(*line_ptr=='\n')){
                break;          //Break the loop
            }
        }
        line_ptr++;     //Increment from delimiter
        
        
        
        /* Field 2 - Name           */
        while(*line_ptr==' '){
            line_ptr++;
        }
        if((*line_ptr=='\0')||(*line_ptr=='\n')){
            break;          //Break the loop
        }
        if(strchr(line_ptr, '\n')){             //See if '\n' can be found
            *(strchr(line_ptr, '\n')) = '\0';   //Replace '\n'
        }
        //Copy name safelly.
        for(j = 0; ((j<SOC_REGISTER_NAME_LENGTH)&&(*line_ptr!='\0')); j++){
            csv_soc_registers_ptr[line_number].register_name[j] = *line_ptr;
            line_ptr++;
        }
        if(j >= SOC_REGISTER_NAME_LENGTH){
            j = (SOC_REGISTER_NAME_LENGTH-1);
            csv_soc_registers_ptr[line_number].register_name[j] = '\0';
        }
    }
    
    if(line_number<(number_of_lines-OMIT_LINES_COUNT)){
        free(csv_soc_registers_ptr);
        fclose(fptr);
        print_error_stderr(ERROR_CSV_PARSING_ERROR);
        fprintf(stderr, "%lu\n", (line_number + 1));
        return ERROR_CSV_PARSING_ERROR;
    }
    
    fclose(fptr);   //Close file
    return line_number;  //Return how many registers where stored
}


/*
 argv[0]    - command
 argv[1]    - inputfile
 argv[2]    - bytes offset
 argv[3]    - bytes count
 argv[4]    - soc type
 argv[>=5]  - optional parameters
 */

#define NUMBER_OF_FIXED_PARAMETERS_INCL_CMDNAME 5 

int main(int argc, char **argv){
    uint32_t temp = 0;
    uint32_t temp2;
    int32_t itemp = 0;
    
    uint32_t bytes_offset = 0;
    uint32_t bytes_count_or_end = 0;
    
#define DATA_ROW_SIZE (4*4)
    uint8_t data_row[DATA_ROW_SIZE];    //Read one "row" 4*4bytes = 16bytes at time
    
    uint32_t addr;
    uint32_t value;
    uint32_t delay;
    uint32_t attr;
    
    int32_t write_flag;
    int32_t read_flag;
    
    uint32_t write_no_bits;
    uint32_t write_start_bit;
    
    uint32_t read_no_bits;
    uint32_t read_start_bit;
    
    uint32_t range_8_10;
    uint32_t range_24_26;
    
    uint32_t selected_soc_type_index = 0;                   //Index in soc_list
    uint32_t closest_register_index;                        //Index in soc_list[selected_soc_type_index].soc_type_registers[]
    
    if(argc < NUMBER_OF_FIXED_PARAMETERS_INCL_CMDNAME){     //argc check
        print_error_stderr(ERROR_PARAMETER_COUNT);                 //Return usage to stderr
        return ERROR_PARAMETER_COUNT;
    }
    
    /* Parse bytes offset - argv[2] */
    if((*argv[2]<48)||(*argv[2]>57)){   //argv[2] must start with a number
        print_error_stderr(ERROR_BYTES_OFFSET_PARAMETER);          //Return bytes offset error to stderr
        return ERROR_BYTES_OFFSET_PARAMETER;
    }
    bytes_offset = strtoul((argv[2]), NULL, 0);                     //Store decimal value       //TODO error handling
    
    
    /* Parse bytes count - argv[3] */
    if((*argv[3]<48)||(*argv[3]>57)){   //argv[3] must start with a number
        print_error_stderr(ERROR_BYTES_COUNT_PARAMETER);           //Return bytes count error to stderr
        return ERROR_BYTES_COUNT_PARAMETER;
    }
    if((argv[3][0]=='0')&&argv[3][1]=='x'){     //If hexadecimal
        bytes_count_or_end = strtoul(argv[3], NULL, 0);            //Store hexadecimal value    //TODO error handling
    }
    else{                               //Else decimal
        bytes_count_or_end = strtoul((argv[3]), NULL, 10);         //Store decimal value        //TODO error handling
        for(temp = 0; argv[3][temp]!='\0';){ //Find string termination
            temp++;
        }
        if(temp>0){
            temp--;                     //Reverse index by 1 to get last char before '\0'
        }
        if(argv[3][temp]=='k'){         //If we find 'k' as last character before string termination
            bytes_count_or_end *= 1024; //Multiply bytes count with 1024
        }
    }
    if(bytes_count_or_end==0){          //Bytes count must be non-zero
        print_error_stderr(ERROR_BYTES_COUNT_PARAMETER);   //Return bytes count error to stderr
        return ERROR_BYTES_COUNT_PARAMETER;  
    }
    if(bytes_count_or_end%16){          //Bytes count must be %128 = 0
        print_error_stderr(ERROR_BYTES_COUNT_PARAMETER);   //Return bytes count error to stderr
        for(temp = 0; temp < bytes_count_or_end;){
            temp+=16;
        }
        if(bytes_count_or_end > 16){
            fprintf(stderr, "Try: %lu or %lu?\n", temp, (temp-16));
        }
        else{
            fprintf(stderr, "Try: %lu ?\n", temp);
        }
        return ERROR_BYTES_COUNT_PARAMETER;
    }
    
    /* Parse Soc Type - argv[4] */
    for(temp = 0; temp<(sizeof(soc_list)/sizeof(soc_type)); temp++){
        if(strcmp(soc_list[temp].soc_type_parameter_str,argv[4])==0){
            selected_soc_type_index = temp;
            break;
        }
    }
    if(temp>=(sizeof(soc_list)/sizeof(soc_type))){                      //If for loop reaches end of list without break;
        print_error_stderr(ERROR_UNKNOWN_SOC_TYPE);
        return(ERROR_UNKNOWN_SOC_TYPE);
    }
    
    /* If SoC Type is "csv" then load cvs file - argv[5] */
    if(strcmp(soc_list[selected_soc_type_index].soc_type_parameter_str,"csv")==0){
        itemp = import_csv_soc_registers(argv[5]);
        if(itemp>0){
            soc_list[selected_soc_type_index].soc_type_registers = csv_soc_registers_ptr;    //Store pointer
            soc_list[selected_soc_type_index].soc_type_registers_count = itemp;                                 //Store length
        }
        else{
            //Prints have been done by the function
            return itemp;
        }
    }
    
    /* Parse potential optional parameters - argv[>=5] or argv[>=6] if csv file is passed as parameter */
    if(process_optional_parameters(argc, argv, (NUMBER_OF_FIXED_PARAMETERS_INCL_CMDNAME+!(strcmp(soc_list[selected_soc_type_index].soc_type_parameter_str,"csv"))))!=0){
        free(csv_soc_registers_ptr);
        print_error_stderr(ERROR_UNKNOWN_OPTIONAL_PARAMETER);
        return ERROR_UNKNOWN_OPTIONAL_PARAMETER;
    }
    
    /* Calculate end of read. Concider bytes_count_or_end as the end of read */
    bytes_count_or_end += bytes_offset;
    
    /* Open File in binary read mode - argv[1] */
    FILE *fptr = NULL;
    fptr = fopen( argv[1] ,"rb");       //Open file

    /* Check that we have a file open */
    if(fptr == NULL){
        free(csv_soc_registers_ptr);
        print_error_stderr(ERROR_OPEN_FILE);       //Return open input file error to stderr
        return ERROR_OPEN_FILE;
    }
    
    /* Check that our range doesn't exceed file */
    fseek(fptr, 0, SEEK_END);                   //Seek the end of file
    if(ftell(fptr)<bytes_count_or_end){
        free(csv_soc_registers_ptr);
        fclose(fptr);
        print_error_stderr(ERROR_RANGE_EXCEEDS_FILE);  //Return range exceeds input file error to stderr
        return ERROR_RANGE_EXCEEDS_FILE;
    }
    
    fseek(fptr, (int32_t)bytes_offset, SEEK_SET);
    
    if(!addresses_only){
        fprintf(stdout, "Start from %lu 0x%x - End to %lu 0x%x - Range %lu 0x%x - Rows %lu \n",
            bytes_offset, bytes_offset,
            bytes_count_or_end, bytes_count_or_end,
            (bytes_count_or_end-bytes_offset), (bytes_count_or_end-bytes_offset),
            ((bytes_count_or_end-bytes_offset)/16)
        );
    }
    
    /* Loop */
    while(bytes_offset<bytes_count_or_end){
        /* Read Row */
        for(uint32_t i = 0; i<DATA_ROW_SIZE; i++){      //TODO Do EOF check
            data_row[i] = fgetc(fptr);
            bytes_offset++;
        }
        
        addr = data_row[0]+(data_row[1]<<8)+(data_row[2]<<16)+(data_row[3]<<24);
        value = data_row[4]+(data_row[5]<<8)+(data_row[6]<<16)+(data_row[7]<<24);
        delay = data_row[8]+(data_row[9]<<8)+(data_row[10]<<16)+(data_row[11]<<24);
        attr = data_row[12]+(data_row[13]<<8)+(data_row[14]<<16)+(data_row[15]<<24);
        
        write_flag = (attr&0x7);
        read_flag = ((attr>>16)&0x7);
    
        write_no_bits = ((attr>>3)&0x1f);
        write_start_bit = ((attr>>11)&0x1f);
    
        read_no_bits = ((attr>>19)&0x1f);
        read_start_bit = ((attr>>27)&0x1f);
        
        range_8_10 =((attr>>8)&0x3);
        range_24_26 =((attr>>24)&0x3);
        
        closest_register_index = get_register_index(addr, soc_list[selected_soc_type_index].soc_type_registers_count, soc_list[selected_soc_type_index].soc_type_registers);
        
        if(!no_address){
            if(!addresses_only){
                change_stdout_green();
                fprintf(stdout, "%s", addr_str);
                change_stdout_default();
            }
        
        fprintf(stdout, "0x%08x", addr);
        }
        
        if(!addresses_only){
            fprintf(stdout, " %-15s", soc_list[selected_soc_type_index].soc_type_registers[closest_register_index].register_name);
            if(print_offset){
                fprintf(stdout, "0x%08x", soc_list[selected_soc_type_index].soc_type_registers[closest_register_index].base_address); 
                fprintf(stdout, "+0x%08x", (addr - soc_list[selected_soc_type_index].soc_type_registers[closest_register_index].base_address));         
            }
            change_stdout_green();
            fprintf(stdout, "%s", value_str);
            change_stdout_default();
            fprintf(stdout, "0x%08x", value);
            change_stdout_green();
            fprintf(stdout, "%s", delay_str);

            if(delay){
                change_stdout_yellow();
            }
            else{
                change_stdout_default();  
            }
        
            fprintf(stdout, "0x%08x", delay);
            fprintf(stdout, " DEC %010lu", delay);
            change_stdout_green();
            fprintf(stdout, "%s", attr_str);
            change_stdout_default();
            fprintf(stdout, "0x%08x  -->", attr);
        
            /* Write Attribute Print */

#define VALID_WRITE_FLAG_4 0x4  //Actual valid flag
#define VALID_WRITE_FLAG_5 0x5  //What is used mostly
#define VALID_NO_WRITE_FLAG 0x0
            if(write_flag){
                if((write_flag==VALID_WRITE_FLAG_4)||(write_flag==VALID_WRITE_FLAG_5)){
                    change_stdout_blue();
                    if(write_flag==VALID_WRITE_FLAG_4){
                        fprintf(stdout, "%s", write_4_str);
                    }
                    else{
                        fprintf(stdout, "%s", write_5_str);
                    } 
                    change_stdout_green();
                    fprintf(stdout, "%s", bit_count_str);
                    change_stdout_default();
                    fprintf(stdout, "%02lu", write_no_bits);
                    change_stdout_green();
                    fprintf(stdout, "%s", bit_start_str);
                    change_stdout_default();
                    fprintf(stdout, "%02lu", write_start_bit);

                    write_flag = VALID_WRITE_FLAG_4;        //Will be used bellow
                }
                else{
                    change_stdout_red();
                    fprintf(stdout, "%s", inv_write_str); 
                }
            
            }

            /* Read Attribute Print */

#define VALID_READ_FLAG_4 0x4  //Actual valid flag
#define VALID_READ_FLAG_5 0x5  //What is used mostly
#define VALID_NO_READ_FLAG 0x0
            if(read_flag){
                if((read_flag==VALID_READ_FLAG_4)||(read_flag==VALID_READ_FLAG_5)){
                    change_stdout_yellow();
                    if(read_flag==VALID_READ_FLAG_4){
                        fprintf(stdout, "%s", read_4_str);
                    }
                    else{
                        fprintf(stdout, "%s", read_5_str);
                    }
                    change_stdout_green();
                    fprintf(stdout, "%s", bit_count_str);
                    change_stdout_default();
                    fprintf(stdout, "%02lu", read_no_bits);
                    change_stdout_green();
                    fprintf(stdout, "%s", bit_start_str);
                    change_stdout_default();
                    fprintf(stdout, "%02lu", read_start_bit);

                    read_flag = VALID_READ_FLAG_4;          //Will be used bellow
                }
                else{
                    change_stdout_red();
                    fprintf(stdout, "%s", inv_read_str); 
                }

            
            }
            else{
                if(!write_flag){                                        //No read or write flags!
                    if(delay){                                          //If delay
                        change_stdout_yellow();
                        fprintf(stdout, "%s", delay_only_str);          //Print Delay only
                        change_stdout_default(); 
                    }
                    else if((addr==0)&&(value==0)&&(attr==0)){          //If we have full null table entry
                        fprintf(stdout, "%s", terminate_str);           //Print terminate
                    }
                    else{
                        change_stdout_red();
                        fprintf(stdout, "%s", none_str);                //Print None(invalid)
                        change_stdout_default();
                    }
                }
            }
        
            /* Extra Notes Part */
            
            temp = (((write_flag==VALID_WRITE_FLAG_4)||(write_flag==VALID_NO_WRITE_FLAG))&&((read_flag==VALID_READ_FLAG_4)||(read_flag==VALID_NO_READ_FLAG))); //0x4 Flag has been set to valid flags above
            temp2 = 0;
            
            if(
            (addr==0 && ( value||delay||attr ))||
            (write_flag==VALID_WRITE_FLAG_4 && read_flag==VALID_READ_FLAG_4)||
            ((write_flag==VALID_WRITE_FLAG_4 && (read_no_bits || read_start_bit)) && temp)||
            ((read_flag==VALID_READ_FLAG_4 && (write_no_bits || read_start_bit)) && temp)||
            (range_8_10 && temp)||
            (range_24_26 && temp)||
            (((write_no_bits + write_start_bit) > 31) && temp)||
            (((read_no_bits + read_start_bit) > 31) && temp)
            ){
                if(attribute_validity_output_format){
                    fprintf(stdout, " ");                   //Some alignment
                }

                change_stdout_red();
                if(addr==0 && ( value||delay||attr )){      //If non-null table entry has null addr
                    if((temp2<number_of_attribute_validity_errors_to_print)&&(attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                        fprintf(stdout, "%s", null_addr_str);
                    }
                    temp2++;
                }
                if(write_flag==VALID_WRITE_FLAG_4 && read_flag==VALID_READ_FLAG_4){ //Both read and write
                    if((temp2<number_of_attribute_validity_errors_to_print)&&(attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                        fprintf(stdout, "%s", both_read_and_write_str);
                    }
                    temp2++;
                }
                if((write_flag==VALID_WRITE_FLAG_4 && (read_no_bits || read_start_bit)) && temp){ //Rogue read parameters
                    if((temp2<number_of_attribute_validity_errors_to_print)&&(attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                        fprintf(stdout, "%s", read_parameters_without_read_flag_str);
                    }
                    temp2++;
                }
                else if((read_flag==VALID_READ_FLAG_4 && (write_no_bits || write_start_bit)) && temp){ //Rogue write parameters
                    if((temp2<number_of_attribute_validity_errors_to_print)&&(attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                        fprintf(stdout, "%s", write_parameters_without_write_flag_str);
                    }
                    temp2++;
                }
                if((range_8_10) && temp){ //Bitfield 8-10 is non-zero
                    if((temp2<number_of_attribute_validity_errors_to_print)&&(attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                        fprintf(stdout, "%s", non_zero_attr_byte_range_8_10_str);
                    }
                    temp2++;
                }
                if((range_24_26) && temp){ //Bitfield 24-26 is non-zero
                    if((temp2<number_of_attribute_validity_errors_to_print)&&(attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                        fprintf(stdout, "%s", non_zero_attr_byte_range_24_26_str);
                    }
                    temp2++;
                }
                if(((write_no_bits + write_start_bit) > 31) && temp){ //Sum exceeds 31
                    if((temp2<number_of_attribute_validity_errors_to_print)&&(attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                        fprintf(stdout, "%s", write_sum_of_count_and_start_exceeds_31_str);
                    }
                    temp2++;
                }
                if(((read_no_bits + read_start_bit) > 31) && temp){ //Sum exceeds 31
                    if((temp2<number_of_attribute_validity_errors_to_print)&&(attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                        fprintf(stdout, "%s", read_sum_of_count_and_start_exceeds_31_str);
                    }
                    temp2++;
                }

                /* If count of omited errors is printed */
                if(print_how_many_attribute_validity_errors_omited && (temp2 > number_of_attribute_validity_errors_to_print) && (attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_PRINT_ERRORS)){
                    fprintf(stdout, "(%lu more)", (temp2-number_of_attribute_validity_errors_to_print));
                }

                /* If only count of errors is to be returned */
                if((temp2) && (attribute_validity_output_format==ATTRIBUTE_VALIDITY_OUTPUT_FORMAT_DETECTED_ERRORS_COUNT)){
                    fprintf(stdout, "(%lu attribute errors)", temp2);
                }
                
            
                change_stdout_default();
            }

        }   //if(!addresses_only)
        
        
        fprintf(stdout, "\n");
    } //while(bytes_offset<bytes_count_or_end)
    
    free(csv_soc_registers_ptr);
    fclose(fptr);
    return 0;
        
}
