
#include "lib_tar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/** * Checks whether the archive is valid.
 * * Each non-null header of a valid archive has:
 * - a magic value of "ustar" and a null,
 * - a version value of "00" and no null,
 * - a correct checksum
 * * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 * * @return a zero or positive value if the archive is valid, representing the number of headers in the archive,
 * -1 if the archive contains a header with an invalid magic value,
 * -2 if the archive contains a header with an invalid version value,
 * -3 if the archive contains a header with an invalid checksum value */

long checksum(tar_header_t *current){
    char *toadd=(char*) current;
    long sum=0;
    for (int i=0;i<148;i++){
        sum+=*toadd;
        toadd++;
    }
    toadd+=8;//we pass the original cheksum
   sum+=256;//header is computed with cheksum seen as space (32 in ascci table). 8*32=256
   for (int i=156;i<513;i++){
        sum+=*toadd;
        toadd++;
    }
   return sum;
}

int check_archive(int tar_fd) {
    puts("checking archive");
    tar_header_t current[sizeof(tar_header_t)];
    int sum_header=0;
    int err=read(tar_fd,(void *) current,sizeof(tar_header_t));
    while (err>0){
        printf("current header:%s\n",current->name);
        if(TAR_INT (current->chksum)!=checksum(current)){
            printf("%ld,%ld\n",TAR_INT (current->chksum),checksum(current));
            return -3;}
        if (strcmp(current->magic,TMAGIC)!=0)
            return -1;
        if (!(*(current->version)=='0'&&*(current->version+1)=='0'))
            return -2;
       sum_header++;
       err=get_next_header(tar_fd,current);
    }
    return sum_header;
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */


int exists(int tar_fd, char *path) {
    tar_header_t current[sizeof(tar_header_t)];
    while(get_next_header(tar_fd,current)>0){
        printf(" path %s\n current:%s\n",path,current->name);
        if(strcmp(current->name,path)==0){
            return 1;
        }
        
        if(current->name[strlen(current->name)-1] == '/'){
            char c = current->name[strlen(current->name)-2];
            
            char noslash[strlen(current->name)];
            memcpy(noslash,&current->name[0],strlen(current->name)-2);
            noslash[strlen(current->name)-2] = c;
            noslash[strlen(current->name)-1] = '\0';
            
            if(strcmp(path,noslash)==0){
                return 1;
            }
        }
        
    }
    return 0;
}
/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 */
int is_dir(int tar_fd, char *path) {
    if(!exists(tar_fd,path))
        return 0;
    lseek(tar_fd, -sizeof(tar_header_t),SEEK_CUR);
    tar_header_t current[sizeof(tar_header_t)];
    read(tar_fd,(void *) current,sizeof(tar_header_t));
    
    if(current->typeflag==DIRTYPE){
        return 1;
    }
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path) {
    if(!exists(tar_fd,path))
        return 0;
    lseek(tar_fd, -sizeof(tar_header_t),SEEK_CUR);
    tar_header_t current[sizeof(tar_header_t)];
    read(tar_fd,(void *) current,sizeof(tar_header_t));
    if(current->typeflag==REGTYPE||current->typeflag==AREGTYPE)
        return 1;
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path) {
    if(!exists(tar_fd,path))
        return 0;
    lseek(tar_fd, -sizeof(tar_header_t),SEEK_CUR);
    tar_header_t current[sizeof(tar_header_t)];
    if(read(tar_fd,(void *) current,sizeof(tar_header_t))==-1)
        fprintf(stderr,"error reading n1");
    if(current->typeflag==SYMTYPE)
        return 1;
    return 0;
}


/**
 * Lists the entries at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry
 * @param no_entries An in-out argument.
 *                   The caller set it to the number of entry in entries.
 *                   The callee set it to the number of entry listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         any other value otherwise.
 */

//compte le nombre de /
int countslashes(char* mystr){
	int len =strlen(mystr);
	int count = 0;
	for(int i = 0; i<len;i++){
		if(mystr[i] == '/')
			count ++;
		}
	return count;
	}
	
// compte la longueur d'un string pour un certains nombre de /	
int count_len(int slash,char*mystr){
	int longueur = 0;
	int slash2 = 0;
	while(longueur<strlen(mystr)){
		if(slash2 == slash){return longueur;}
		if(mystr[longueur] == '/'){
			slash2 ++;
			}
		longueur ++;
		}
	return longueur;
	}


int list(int tar_fd, char *path, char **entries, size_t *no_entries) {
	
	if(strcmp(path,"")==0){
		*no_entries = 0;
		return 0;
	}
		
	if(!exists(tar_fd,path)){
	printf("\n %s nexiste pas \n",path);	
	*no_entries = 0;
	return 0;
	}
	lseek(tar_fd,0,SEEK_SET);
	//cas des symlink
	if(is_symlink(tar_fd,path)){
		lseek(tar_fd, -sizeof(tar_header_t),SEEK_CUR);
		tar_header_t current[sizeof(tar_header_t)];
		if(read(tar_fd,(void *) current,sizeof(tar_header_t))==-1)
			fprintf(stderr,"error reading n1");
		int slashpath = countslashes(path);
		int lenpath1 = count_len(slashpath,path);
		// ok cas direct si le path ne contient pas de / en return avec le linkname
		if (slashpath == 0){
			char pathlink[strlen(current->linkname)];
			strcpy(pathlink,current->linkname);
			if(pathlink[strlen(current->linkname)] !='/'){
			pathlink[strlen(current->linkname)] ='/';
			pathlink[strlen(current->linkname)+1] ='\0';}
			lseek(tar_fd,0,SEEK_SET);
			return list(tar_fd,pathlink,entries,no_entries);
		}
		//ok cas en arrière si il y a ../ dans le linkname il faut retourner en arriere dans le path
		if(current->linkname[0] == '.' && current->linkname[1] == '.'){
			//dimminution du path
			slashpath -=1;
			int len2 = count_len(slashpath,path);
			char path1[len2];
			char path2[strlen(current->linkname)];
			memcpy(path1,&path[0],len2);
			memcpy(path2,&current->linkname[3],strlen(current->linkname));
			strcat(path1,path2);
			//il faut gérer le cas ou le linkname n'a pas de / à la fin
			if(path1[strlen(path1)] !='/'){
				path1[strlen(path1)] ='/';
				path1[strlen(path1)] ='\0';
			}
			lseek(tar_fd,0,SEEK_SET);
			return list(tar_fd,path1,entries,no_entries);
		}
		//les autres cas(return path+linkname)
		else{
			char path1[lenpath1];
			char path2[strlen(current->linkname)];
			memcpy(path1,&path[0],lenpath1);
			path1[lenpath1] ='\0';
			memcpy(path2,&current->linkname[0],strlen(current->linkname));
			path2[strlen(path2)] ='\0';
			strcat(path1,path2);
			if(path1[lenpath1+strlen(current->linkname)] !='/'){
				path1[lenpath1+strlen(current->linkname)] ='/';
				path1[lenpath1+strlen(current->linkname)+1] ='\0';
			}
			printf("%s le path1 ",path1);
			lseek(tar_fd,0,SEEK_SET);
			return list(tar_fd,path1,entries,no_entries);
		}
	}
	lseek(tar_fd,0,SEEK_SET);
	if(is_dir(tar_fd,path)){
		//si le dossier n'as pas de / à la fin
		if(path[strlen(path)-1] != '/'){
			char mystr[strlen(path)+1];
			strcpy(mystr,path);
			strcat(mystr,"/\0");
			lseek(tar_fd,0,SEEK_SET);
			return list(tar_fd,mystr,entries,no_entries);
		}
		printf("pour un path correspondant à %s \n",path);		
		lseek(tar_fd,0,SEEK_SET);
		//on compte les slash du path ainsi que la longueur jusqu'au dossier indiqué
		int slash = countslashes(path);
		tar_header_t current[sizeof(tar_header_t)];
		int count = 0;
		while(get_next_header(tar_fd,current)>0){
			//compteur dépassé
			if(count>= *no_entries){
				*no_entries = count;
				return 1;
			}	
			//conditions si le header est un fichier + conditions pour ne pas aller plus loin + cas début de l'archive	
			if(countslashes(current->name) == 1 && slash ==0&& count_len(slash+1,current->name)>=strlen(current->name)){
				char buff[strlen(current->name)-count_len(slash,current->name)];
				memcpy(buff,&current->name[count_len(slash,current->name)-1],strlen(current->name)-count_len(slash,current->name)+1);
				buff[strlen(current->name)-count_len(slash,current->name)+1] = '\0';
				strcpy(entries[count],buff);
				printf("l'entrée folder %d,est %s \n",count,entries[count]);
				count++;			
			}
			//conditions si le header est un fichier + conditions pour ne pas aller plus loin + cas debut de l'archive
			if(countslashes(current->name) == 0 && slash ==0 && strlen(current->name) >0){
				char buff[strlen(current->name)-count_len(slash,current->name)];
				memcpy(buff,&current->name[count_len(slash,current->name)-1],strlen(current->name)-count_len(slash,current->name)+1);
				buff[strlen(current->name)-count_len(slash,current->name)+1] = '\0';		
				strcpy(entries[count],buff);
				printf("l'entrée file %d,est %s \n",count,entries[count]);
				count++;
			}
			//cas dossier avec un path contenant des /
			if(countslashes(current->name) == slash+1 && slash !=0 && count_len(slash+1,current->name)>= strlen(current->name)){
				char buff[strlen(current->name)-count_len(slash,current->name)];
				memcpy(buff,&current->name[count_len(slash,current->name)],strlen(current->name)-count_len(slash,current->name)+1);
				buff[strlen(current->name)-count_len(slash,current->name)] = '\0';		
				strcpy(entries[count],buff);
				printf("l'entrée %d,est %s \n",count,entries[count]);
				count++;						
			}
			//cas du fichier avec path contenant des /
			if(countslashes(current->name) == slash && slash !=0 && count_len(slash,current->name)<strlen(current->name)){
				char buff[strlen(current->name)-count_len(slash,current->name)];
				memcpy(buff,&current->name[count_len(slash,current->name)],strlen(current->name)-count_len(slash,current->name)+1);
				buff[strlen(current->name)-count_len(slash,current->name)] = '\0';
				strcpy(entries[count],buff);
				printf("l'entrée %d,est %s \n",count,entries[count]);
				count++;
			}	
		}
	//tous les header sont parcourus		
	*no_entries = count;
	return 1;	
	}
	*no_entries = 0;
	return 0;
}
/**
 * Reads a file at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a tar archive file.
 * @param path A path to an entry in the archive to read from.  If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read.
 *
 */
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len) {
    if(!exists(tar_fd,path)){
        return -1;
    }
    lseek(tar_fd,-(sizeof(tar_header_t)),SEEK_CUR);
    tar_header_t current[sizeof(tar_header_t)];
    read(tar_fd,(void *) current,sizeof(tar_header_t));
    printf("reading from  %s \n",current->name);
    printf("current size: %ld \n",TAR_INT(current->size));
    int size=TAR_INT(current->size);
    if(current->typeflag == SYMTYPE){
		printf("c'est un symlink \n");
		int slashpath = countslashes(path);
		int lenpath1 = count_len(slashpath,path);
		// ok cas direct si le path ne contient pas de / en return avec le linkname
		if (slashpath == 0){
			char pathlink[strlen(current->linkname)];
			strcpy(pathlink,current->linkname);
			lseek(tar_fd,0,SEEK_SET);
			return read_file(tar_fd,pathlink,offset,dest,len);
		}
		//ok cas en arrière si il y a ../ dans le linkname il faut retourner en arriere dans le path
		if(current->linkname[0] == '.' && current->linkname[1] == '.'){
			//dimminution du path
			slashpath -=1;
			int len2 = count_len(slashpath,path);
			char path1[len2];
			char path2[strlen(current->linkname)];
			memcpy(path1,&path[0],len2);
			memcpy(path2,&current->linkname[3],strlen(current->linkname));
			strcat(path1,path2);
			//il faut gérer le cas ou le linkname n'a pas de / à la fin
			lseek(tar_fd,0,SEEK_SET);
			return read_file(tar_fd,path1,offset,dest,len);
		}
		//les autres cas(return path+linkname)
		else{
			char path1[lenpath1];
			memcpy(path1,&path[0],lenpath1);
			path1[lenpath1]='\0';
			strcat(path1,current->linkname);
			lseek(tar_fd,0,SEEK_SET);
			return read_file(tar_fd,path1,offset,dest,len);
			}
	}
    if((!(current->typeflag==REGTYPE))&&(!(current->typeflag==AREGTYPE))){
         return -1;
    }
    if (offset>=size){
       return -2;
    }
    if(size-offset<=*len){
        *len=size-offset;
    }
    lseek(tar_fd,offset,SEEK_CUR);
    *len=read(tar_fd,(void *) dest,*len);
    if(*len !=size-offset){
       printf ("size readable from offset: %ld\n read:%ld\n diff:%ld\n",size-offset,*len,size-offset-*len);
        return size-offset-*len;
    }
   return 0;

}

int get_next_header(int tar_fd, tar_header_t *current){
    int size=correct_padding(TAR_INT(current->size));
    lseek(tar_fd, size,SEEK_CUR);
    int err=read(tar_fd,(void *) current,sizeof(tar_header_t));
    if (checksum(current)==256){
        return -2;
    }
   return err;
}

int correct_padding(int size){
    if (size%512!=0){
        size+=512-(size%512);
    }
    return size;
} 
