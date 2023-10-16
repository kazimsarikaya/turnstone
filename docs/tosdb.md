# TURNSTONE Operating System Database {#tosdb}

Turnstone operating system stores all its data inside itsown database architecture named as **TOSDB**. TOSDB uses raw partitions of **gpt** disk, memory, or file at build host. 

TOSDB stores it all data within blocks which have header tosdb_block_header_t. Each block is chained and referes previous related block. Because of old blocks are readonly, block header tags previous block valid or not. 
