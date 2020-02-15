# hisi-initregtable-parser
Make them binary blobs human readable.

Build: gcc -Wall -g hisi-initregtable-parser.c -o hisi-initregtable-parser

Parses HiSilicon SoC register tables(in binary format) used in bootloader(u-boot) with early low level function:
init_registers(uint32_t* table_start_address, uint32_t mode)

Main purpose of this early low level function is to configure clocks and ddr phy/mem configuration in early stage so bootloader can be loaded in ddr ram. Also io muxing is partially or completelly done at this phase.

- Parsed init register table can be used to make educated guesses about undocumented hardware configuration! 

Uses external per SoC type csv-files to identify which register base table entry refers to.
Any contribution in terms of accurate&complete csv-files for different devices is much appreciated.

Base+Offset can be printed with -printoffsets

Address values only can be printed with -addronly
 - Can be used to fetch values from running platform for comparison!

More details about blobs, init_registers() and how to use this tool inside .c source.

Colored mode and -nocolor for use with external tools
![Colored mode and nocolor for use with external tools](https://raw.githubusercontent.com/kakigate/hisi-initregtable-parser/master/pics/hisi-initregtable-parser.png)

Example use with external tool - visual diff of two tables with Meld
![2 Parses analyzed in Meld](https://github.com/kakigate/hisi-initregtable-parser/blob/master/pics/hisi-initregtable-parse-meld.png?raw=true)
