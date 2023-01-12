#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "hicpp-signed-bitwise"
#pragma ide diagnostic ignored "readability-non-const-parameter"

#include "main.h"


uint8_t ilog2(uint32_t n) {
    uint8_t i = 0;
    while ((n >>= 1U) != 0)
        i++;
    return i;
}

//--------------------------------------------------------------------------------------------------------
//                                           DEBUT DU CODE
//--------------------------------------------------------------------------------------------------------

/**
 * Exercice 1
 *
 * Prend cluster et retourne son addresse en secteur dans l'archive
 * @param block le block de paramètre du BIOS
 * @param cluster le cluster à convertir en LBA
 * @param first_data_sector le premier secteur de données, donnée par la formule dans le document
 * @return le LBA
 */
uint32_t cluster_to_lba(BPB *block, uint32_t cluster, uint32_t first_data_sector) {
    uint32_t begin = as_uint16(block->BPB_RsvdSecCnt) +
                     as_uint32(block->BPB_HiddSec) +
                     (block->BPB_NumFATs * as_uint32(block->BPB_FATSz32));

    uint32_t lba = begin + (cluster - as_uint32(block->BPB_RootClus)) * block->BPB_SecPerClus;

    return lba;
}

/**
 * Exercice 2
 *
 * Va chercher une valeur dans la cluster chain
 * @param block le block de paramètre du système de fichier
 * @param cluster le cluster qu'on veut aller lire
 * @param value un pointeur ou on retourne la valeur
 * @param archive le fichier de l'archive
 * @return un src d'erreur
 */
error_code get_cluster_chain_value(BPB *block,
                                   uint32_t cluster,
                                   uint32_t *value,
                                   FILE *archive) {
    if (archive == NULL) { return -1; }
    uint32_t fStart = as_uint16(block->BPB_RsvdSecCnt) + as_uint32(block->BPB_HiddSec); //secteurs réservés+cachés

    // fStart * (Sector / Cluster) * (Byte / Sector)
    uint32_t cStart = ( fStart * (block->BPB_SecPerClus) * as_uint16(block->BPB_BytsPerSec) ) + cluster * 4;

    fseek(archive, cStart, SEEK_SET);

    uint32_t value_array[4];

    int i;
    for(i = 0; i < 4; i++) {
        value_array[i] = fgetc(archive);
    }

    *value = as_uint32(value_array);

    return 0;
}


/**
 * Exercice 3
 *
 * Vérifie si un descripteur de fichier FAT identifie bien fichier avec le nom name
 * @param entry le descripteur de fichier
 * @param name le nom de fichier
 * @return 0 ou 1 (faux ou vrai)
 */
bool file_has_name(FAT_entry *entry, char *name) {

    if (strlen(name) > 12) {return 0;}

    char temp[12];
    temp[11] = '\0';
    memset(temp, 0x20, 11);

    //Cas où on a . ou ..
    if (strlen(name) == 1 && name[0] == '.') {
        temp[0] = name[0];
    } else if (strlen(name) == 2 && name[0] == '.' && name[1] == '.') {
        temp[0] = name[0];
        temp[1] = name[1];
    } else {

        int i = 0; //position dans temp
        int j = 0; //position dans name
        while (name[j] != '\0') {
            if (name[j] == '.') {
                i = 7;
            } else {
                temp[i] = toupper(name[j]);
            }
            i++;
            j++;
        }
    }

    //Comparer l'entrée et temp
    for (int i=0; i<11;i++) {
        if (entry->DIR_Name[i] != temp[i]) {
            //printf("i:%d  %X != %X \n", i, entry->DIR_Name[i], temp[i]);
            return 0;
        }
    }

    return 1;
}

/**
 * Exercice 4
 *
 * Prend un path de la forme "/dossier/dossier2/fichier.ext et retourne la partie
 * correspondante à l'index passé. Le premier '/' est facultatif.
 * @param path l'index passé
 * @param level la partie à retourner (ici, 0 retournerait dossier)
 * @param output la sortie (la string)
 * @return -1 si les arguments sont incorrects, -2 si le path ne contient pas autant de niveaux
 * -3 si out of memory
 */
error_code break_up_path(char *path, uint8_t level, char **output) {

    //Arguments incorrects:
    if (strlen(path) == 0 || path == NULL || level < 0) {
        return -1;
    }

    *output = malloc(13*sizeof(char));
    if (output == NULL) {return -3;}
    uint32_t i=0; //index entire path
    uint32_t j=0; //index output
    uint8_t index_path = 0; //current level

    //Si le premier caractère est / sauter au prochain
    if (path[0] == '/') {
        i++;
    }

    while(path[i] != '\0'){
        if (path[i] == '/') {
            index_path++;
            //printf("new index_path: %d\n",index_path);
        } else if (index_path == level) {
            (*output)[j] = toupper(path[i]);
            j++;
        } else if (index_path > level) {
            break;
        }
        i++;
    }

    if (level > index_path) {
        return -2;
    }

    (*output)[j] = '\0';

    return strlen(*output);
}


/**
 * Exercice 5
 *
 * Lit le BIOS parameter block
 * @param archive fichier qui correspond à l'archive
 * @param block le block alloué
 * @return un src d'erreur
 */
error_code read_boot_block(FILE *archive, BPB **block) {
    if (archive == NULL) { return -1; }
    *block = malloc( sizeof(BPB) );
    if (block == NULL) {
        exit(1);
    }
    BPB *sb = *block;
    int i;
    for (i = 0; i < 3; i++) {
        sb->BS_jmpBoot[i] = getc(archive);
    }
    for (i = 3; i < 11; i++) {
        sb->BS_OEMName[i-3] = getc(archive);
    }
    for (i = 11; i < 13; i++) {
        sb->BPB_BytsPerSec[i-11] = getc(archive);
    }
    sb->BPB_SecPerClus = getc(archive);
    for (i = 14; i < 16; i++) {
        sb->BPB_RsvdSecCnt[i-14] = getc(archive);
    }
    sb->BPB_NumFATs = getc(archive);
    for (i = 17; i < 19; i++) {
        sb->BPB_RootEntCnt[i-17] = getc(archive);
    }
    for (i = 19; i < 21; i++) {
        sb->BPB_TotSec16[i-19] = getc(archive);
    }
    sb->BPB_Media = getc(archive);
    for (i = 22; i < 24; i++) {
        sb->BPB_FATSz16[i-22] = getc(archive);
    }
    for (i = 24; i < 26; i++) {
        sb->BPB_SecPerTrk[i-24] = getc(archive);
    }
    for (i = 26; i < 28; i++) {
        sb->BPB_NumHeads[i-26] = getc(archive);
    }
    for (i = 28; i < 32; i++) {
        sb->BPB_HiddSec[i-28] = getc(archive);
    }
    for (i = 32; i < 36; i++) {
        sb->BPB_TotSec32[i-32] = getc(archive);
    }
    for (i = 36; i < 40; i++) {
        sb->BPB_FATSz32[i-36] = getc(archive);
    }
    for (i = 40; i < 42; i++) {
        sb->BPB_ExtFlags[i-40] = getc(archive);
    }
    for (i = 42; i < 44; i++) {
        sb->BPB_FSVer[i-42] = getc(archive);
    }
    for (i = 44; i < 48; i++) {
        sb->BPB_RootClus[i-44] = getc(archive);
    }
    for (i = 48; i < 50; i++) {
        sb->BPB_FSInfo[i-48] = getc(archive);
    }
    for (i = 50; i < 52; i++) {
        sb->BPB_BkBootSec[i-50] = getc(archive);
    }
    for (i = 52; i < 64; i++) {
        sb->BPB_Reserved[i-52] = getc(archive);
    }
    sb->BS_DrvNum = getc(archive);
    sb->BS_Reserved1 = getc(archive);
    sb->BS_BootSig = getc(archive);
    for (i = 67; i < 71; i++) {
        sb->BS_VolID[i-67] = getc(archive);
    }
    for (i = 71; i < 82; i++) {
        sb->BS_VolLab[i-71] = getc(archive);
    }
    for (i = 82; i < 90; i++) {
        sb->BS_FilSysType[i-82] = getc(archive);
    }


    return 0;
}

/**
 * Exercice 6
 *
 * Trouve un descripteur de fichier dans l'archive
 * @param archive le descripteur de fichier qui correspond à l'archive
 * @param path le chemin du fichier
 * @param entry l'entrée trouvée
 * @return un src d'erreur
 */
error_code find_file_descriptor(FILE *archive, BPB *block, char *path, FAT_entry **entry) {

    //  Ex:  /spanish/los.txt

    //Trouver la profondeur totale du path
    int max_depth = 0;
    uint32_t i = 0;
    //Si premier caractère est /, sauter
    if (path[0] == '/') {
        i++;
    }
    while (path[i] != '\0') {
        if (path[i] == '/') {
            max_depth++;
        }
        i++;
    }

    //Créer un FAT_entry et changer seulement les infos au fur et à mesure, mais garder le même
    *entry = malloc(sizeof(FAT_entry));

    //On commence au dossier root
    uint32_t position_bytes = cluster_to_lba(block, as_uint32(block->BPB_RootClus), NULL) * (uint32_t)as_uint16(block->BPB_BytsPerSec);

    //Mettre à la position du root
    fseek(archive, position_bytes, SEEK_SET);

    for (int d=0; d<=max_depth;d++) {

        char *level_name;
        //get string
        break_up_path(path, d, &level_name);


        do {
            //Copier une entrée dans entry
            for (int i = 0; i < 32; i++) {
                if (i < 11) {
                    (*entry)->DIR_Name[i] = getc(archive); //pas de \0
                } else if (i < 12) {
                    (*entry)->DIR_Attr = getc(archive);
                } else if (i < 13) {
                    (*entry)->DIR_NTRes = getc(archive);
                } else if (i < 14) {
                    (*entry)->DIR_CrtTimeTenth = getc(archive);
                } else if (i < 16) {
                    (*entry)->DIR_CrtTime[i-14] = getc(archive);
                } else if (i < 18) {
                    (*entry)->DIR_CrtDate[i-16] = getc(archive);
                } else if (i < 20) {
                    (*entry)->DIR_LstAccDate[i-18] = getc(archive);
                } else if (i < 22) {
                    (*entry)->DIR_FstClusHI[i-20] = getc(archive);
                } else if (i < 24) {
                    (*entry)->DIR_WrtTime[i-22] = getc(archive);
                } else if (i < 26) {
                    (*entry)->DIR_WrtDate[i-24] = getc(archive);
                } else if (i < 28) {
                    (*entry)->DIR_FstClusLO[i-26] = getc(archive);
                } else {
                    (*entry)->DIR_FileSize[i-28] = getc(archive);
                }
            }


            if ((*entry)->DIR_Name[0] == 0) {
                //On est arrivés au bout du dossier et on n'a pas trouvé

                free(level_name);
                return -1;
            }

            /*Si on trouve:
             *      -Niveau max depth:
             *              -dossier -> erreur (free level_name)
             *              -fichier -> good (free level_name)
             *      -Pas max depth:
             *              -dossier -> CONTINUER la recherche (free level_name)
             *              -fichier -> ERREUR (free level_name)
            */



            //Si on trouve
            if (file_has_name(*entry,level_name)) {


                bool est_dossier = ((*entry)->DIR_Attr & 0x10) != 0;

                free(level_name);

                //Si on est à la profondeur maximale:
                if (d >= max_depth) {

                    if (est_dossier) {
                        return -1; //Erreur
                    } else {
                        return 0; //Succès
                    }
                }

                //Si pas à la profondeur maximale:
                else {

                    if (est_dossier) { //CONTINUER

                        uint32_t cluster_high = as_uint16((*entry)->DIR_FstClusHI);
                        uint32_t cluster_low = as_uint16((*entry)->DIR_FstClusLO);


                        uint32_t cluster = (cluster_high << 16) + cluster_low;
                        cluster = cluster&0x0FFFFFFF; //Masque

                        //ATTENTION: si cluster est 0, c'est le root directory
                        if (cluster == 0) {cluster = as_uint32(block->BPB_RootClus);};


                        position_bytes = cluster_to_lba(block, cluster, NULL) * (uint32_t)as_uint16(block->BPB_BytsPerSec);

                        fseek(archive, position_bytes, SEEK_SET);

                        break;
                    } else { //Si fichier: Erreur

                        return -1;
                    }
                }
            }

        } while(1);

    }

    return 0;
}

/**
 * Exercice 7
 *
 * Lit un fichier dans une archive FAT
 * @param entry l'entrée du fichier
 * @param buff le buffer ou écrire les données
 * @param max_len la longueur du buffer
 * @return un src d'erreur qui va contenir la longueur des donnés lues
 */
error_code
read_file(FILE *archive, BPB *block, FAT_entry *entry, void *buff, size_t max_len) {

    //Si entry pas valide -> return -1
    if (entry->DIR_Name[0] == 0) {
        return -1;
    }

    uint32_t bytes_per_cluster = as_uint16(block->BPB_BytsPerSec) * block->BPB_SecPerClus;


    size_t taille_fichier = as_uint32(entry->DIR_FileSize);

    uint32_t write_count = 0;

    uint32_t cluster_high = as_uint16(entry->DIR_FstClusHI);
    uint32_t cluster_low = as_uint16(entry->DIR_FstClusLO);
    uint32_t cluster = (cluster_high << 16) + cluster_low;

    cluster = cluster&0x0FFFFFFF; //Masque


    uint32_t position_bytes = cluster_to_lba(block, cluster, NULL) * (uint32_t)as_uint16(block->BPB_BytsPerSec);
    fseek(archive, position_bytes, SEEK_SET);



    while(write_count < taille_fichier && write_count < max_len) {
        ((char*) buff)[write_count] = getc(archive);
        write_count++;

        //Si on est arrivé au bout d'un cluster, changer de cluster:
        if (write_count % bytes_per_cluster == 0) {

            //Trouver position en bytes du cluster actuel dans FAT table:
            uint32_t position_cluster_bytes = (as_uint16(block->BPB_RsvdSecCnt) + as_uint32(block->BPB_HiddSec))*bytes_per_cluster+cluster*4;
            fseek(archive, position_cluster_bytes, SEEK_SET);

            uint8_t next[4];

            //Lire next cluster:
            for (int i=0; i < 4; i++) {
                next[i] = getc(archive);
            }
            cluster = as_uint32(next);
            cluster = cluster&0x0FFFFFFF; //Masque


            position_bytes = cluster_to_lba(block, cluster, NULL) * (uint32_t)as_uint16(block->BPB_BytsPerSec);
            fseek(archive, position_bytes, SEEK_SET);
        }
    };


    return write_count;
}

// ༽つ۞﹏۞༼つ

int main() {
// vous pouvez ajouter des tests pour les fonctions ici


    BPB *block;
    FILE *archive = fopen("../../floppy.img", "r");
    if (archive == NULL) {
        exit(-1);
    }

    read_boot_block(archive, &block);


    FAT_entry *entry;

    char path[] = "spanish/los.txt";

    int code = find_file_descriptor(archive, block, path, &entry);

    printf("MAIN nom fichier: %s\n",entry->DIR_Name);
    printf("code sortie: %d\n",code);


    void *buff = malloc(2000*sizeof(char));
    memset(buff, '\0', sizeof(buff)); //nécessaire?

    read_file(archive, block, entry, buff,2000);

    printf("contenu: %s\n",buff);
    free(block);
    free(entry);
    free(buff);

    return 0;
}

