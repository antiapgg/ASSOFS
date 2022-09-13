#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include <asm/uaccess.h>        /* copy_to_user          */
#include <linux/sched.h>
#include "assoofs.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Antía Pérez-Gorostiaga González.");

  /* ----------------------------------------------------------------------------------------- */
 /* -------------------------------------- DECLARACION -------------------------------------- */
/* ----------------------------------------------------------------------------------------- */

/**********************************************************************************************
 *                                 Operaciones sobre ficheros                                 *
 **********************************************************************************************/

ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos);

ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos);

const struct file_operations assoofs_file_operations = {
    .read = assoofs_read,
    .write = assoofs_write,
};

/**********************************************************************************************
 *                               Operaciones sobre directorios                                *
 **********************************************************************************************/

static int assoofs_iterate(struct file *filp, struct dir_context *ctx);

const struct file_operations assoofs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate = assoofs_iterate,
};

/**********************************************************************************************
 *                                   Operaciones sobre inodos                                 *
 **********************************************************************************************/

static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);

static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);

static struct inode_operations assoofs_inode_ops = {
    .create = assoofs_create,
    .lookup = assoofs_lookup,
    .mkdir = assoofs_mkdir,
};

/**********************************************************************************************
 *                               Operaciones sobre el superbloque                             *
 **********************************************************************************************/

static const struct super_operations assoofs_sops = {
    .drop_inode = generic_delete_inode,
};

/**********************************************************************************************
 *                                   Funciones Auxiliares                                      *
 **********************************************************************************************/

/** Declaro Struct assoofs_get_inode_info (2.3.3) **/
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no);

/** Declaro Struct assoofs_get_inode (2.3.4) **/
static struct inode *assoofs_get_inode(struct super_block *sb, int ino);
/** Declaro funcion assoofs_sb_get_a_freeblock (2.3.4) **/
int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block);

/** Declaro funcion assoofs_save_sb_info (2.3.4) **/
void assoofs_save_sb_info(struct super_block *vsb);

/** Declaro funcion assoofs_add_inode_info (2.3.4) **/
void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode);

/** Declaro funcion assoofs_save_inode_info (2.3.4) **/
int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info);

/** Declaro funcion assoofs_search_inode_info (2.3.4) **/
struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search);


  /* ----------------------------------------------------------------------------------------- */
 /* --------------------------------------- FUNCIONES --------------------------------------- */
/* ----------------------------------------------------------------------------------------- */

/**********************************************************************************************
 *                                 Operaciones sobre ficheros                                 *
 **********************************************************************************************/

/******************************* Leer un archivo  *******************************/
ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos) {
   
    //DECLARACIONES
    int nbytes = 0;
    //Obtengo la informacion persistente del inodo a partir de filp
    struct assoofs_inode_info *inode_info = filp->f_path.dentry->d_inode->i_private;
    struct buffer_head *bh;
    char *buffer;
    
    printk(KERN_INFO "\n********** Llamada a Read **********\n");
    
    //Compruebo el valor de ppos por si alcanzo el final del fichero
    if(*ppos >= inode_info->file_size){
        printk(KERN_INFO "      READ - Final del Fichero Alcanzado.\n");
        return 0;
    }
    
    //Accedo al contenido del fichero
    bh = sb_bread(filp->f_path.dentry->d_inode->i_sb, inode_info->data_block_number);
    
    if(!bh){
        printk(KERN_INFO "      READ - Fallo en la lectura del bloque.\n");
        return 0;
    }
    
    buffer = (char *)bh->b_data;
    
    //Comparo len con el tamaño del fichero por si llego al final
    nbytes = min((size_t) inode_info->file_size, len);
    
    //Copio en el buffer el contenido del fichero leido en el paso anterior
    if(copy_to_user(buf, buffer, nbytes)){
        //Libero
        brelse(bh);
        printk(KERN_INFO "      READ - Error copiando el contenido del fichero al buffer de espacio usuario.\n");
        return -EFAULT;
    }
    
    brelse(bh);
    
    //Incremento el valor de ppos
    *ppos += nbytes;
    
    printk(KERN_INFO "********** Fin llamada a Read **********\n");
    
    //Devuelvo el numero de bytes leidos
    return nbytes;
}

/******************************* Escribir en un archivo *******************************/
ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos) {
      
    //DECLARACIONES
     //Obtengo la informacion persistente del inodo a partir de filp
    struct assoofs_inode_info *inode_info = filp->f_path.dentry->d_inode->i_private;
    struct buffer_head *bh;
    struct super_block *sb;
    char *buffer;
    int aux = 0;
    
    printk(KERN_INFO "\n********** Llamada a Write **********\n");
    
    printk(KERN_INFO "      WRITE - Intentamos escribir %lu Bytes en el inodo %lu.\n", len, filp -> f_path.dentry -> d_inode -> i_ino);
    
    sb = filp->f_path.dentry->d_inode->i_sb;
    
    if (aux) return aux;
    
    //Compruebo el valor de ppos por si alcanzo el final del fichero
    bh = sb_bread(filp->f_path.dentry->d_inode->i_sb, inode_info->data_block_number);
    
    if (!bh) {
        printk(KERN_ERR "      WRITE -  Leyendo el numero de bloque [%llu] failed.\n", inode_info -> data_block_number);
        return 0;
    }
    
    buffer = (char *) bh -> b_data;
    printk(KERN_INFO "      WRITE - Valor del Buffer = %s.\n", buffer);
    buffer += *ppos;
    
    //Escribo en el fichero los datos obtenidos de buf 
    if(copy_from_user(buffer, buf, len)){
        //Libero
        brelse(bh);
        printk(KERN_INFO "      WRITE - Error copiando el contenido del buffer de espacio usuario al espacio kernel.\n");
        return -EFAULT;
    }
    
    //Incremento el valor de ppos
    *ppos += len;
    
    //Marco el bloque como sucio
    mark_buffer_dirty(bh);
    //Sincronizo
    sync_dirty_buffer(bh);
    //Libero
    brelse(bh);
    
    //Actualizo el campo file_size de la informacion persistente en el nodo
    inode_info->file_size = *ppos;
    
    printk(KERN_INFO "      WRITE - Guardando inode_no. %llu .\n", inode_info->inode_no);
    
    aux = assoofs_save_inode_info(sb, inode_info);
    
    if (aux) len = aux;
    
    printk(KERN_INFO "      WRITE - Escribiendo %lu Bytes en inodo %lu.\n",  len, filp -> f_path.dentry -> d_inode -> i_ino);
    
    printk(KERN_INFO "********** Fin llamada a Write **********\n");
    
    //Devuelvo el numero de bytes escritos
    return len;
}


/**********************************************************************************************
 *                               Operaciones sobre directorios                                *
 **********************************************************************************************/

/******************************* Mostrar contenido de directorio ******************************/
static int assoofs_iterate(struct file *filp, struct dir_context *ctx) {
    
    //DECLARACIONES
    struct inode *inode;
    struct super_block *sb;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    struct assoofs_inode_info *inode_info;
    int i = 0;
    
    printk(KERN_INFO "\n********** Llamada a Iterate **********\n");
    
    /** 1. Accedo al inodo, a la indo persist del inodo, y al superbloq correcpond al arg filp **/
    inode = filp->f_path.dentry->d_inode;
    sb = inode->i_sb;
    inode_info = inode->i_private;
    
    /** 2. Compruebo si el contexto del directorio ya esta creado **/
    if(ctx->pos) return 0;
    
    /** 3. Compruebo que el inodo obtenido en el paso 1 se corresponde con un directorio **/
    if((!S_ISDIR(inode_info->mode))) return -1;
    
    /** 4. Accedo al bloq dd se almacena el cont del dir y con la info que cont ini el contex ctx **/
    bh = sb_bread(sb, inode_info->data_block_number);
    record = (struct assoofs_dir_record_entry *) bh->b_data;
    
    for (i = 0; i < inode_info->dir_children_count; i++){
    
    	dir_emit(ctx, record->filename, ASSOOFS_FILENAME_MAXLEN, record->inode_no, DT_UNKNOWN);
    	ctx->pos += sizeof(struct assoofs_dir_record_entry);
    	record++;
    }
    //Libero
    brelse(bh);
    
    printk(KERN_INFO "********** Fin llamada a Iterate **********\n");

    return 0;
}


/**********************************************************************************************
 *                                   Operaciones sobre inodos                                 *
 **********************************************************************************************/

/************************** Creación de nuevos inodos para archivos ****************************/
static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    
    //DECLARACIONES
    int aux;
    uint64_t count;
    struct inode *inode;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct assoofs_dir_record_entry *dir_contents;
    struct super_block *sb;
    struct buffer_head *bh;
    
    printk(KERN_INFO "\n********** Llamada a Create **********\n");
    
    /** 1. Creo nuevo inodo **/
    // obtengo un puntero al superbloque desde dir
    sb = dir->i_sb;
    
    // obtengo el número de inodos de la informacion persistente del superbloque
    count = ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count;
    
    //Compruebo que el valor de count no es superior al numero maximo de objetos soportados
    if(count >= ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED){
        printk(KERN_ERR"ERROR, El archivo esta completo.\n" );
        return -12;
    }
    
    //Nuevo inodo
    inode = new_inode(sb);
    
    // Asigno numero al nuevo inodo a partir de count
    inode->i_ino = (count + ASSOOFS_START_INO - ASSOOFS_RESERVED_INODES + 1);
        
    //Guardo el superbloque en el inodo
    inode->i_sb = sb;
    
    inode->i_op = &assoofs_inode_ops;
            
    // fechas.
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);      
    
    inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
    
    // El segundo mode me llega como argumento
    inode_info->mode = mode;
    
    inode_info->inode_no = inode->i_ino; /** hhhhhh **/
    
    printk(KERN_INFO "      CREATE - Solicitud de creación de nuevo archivo %s.", dentry -> d_name.name);
    
    inode_info->file_size = 0;
    
    //Para las operaciones sobre ficheros
    inode->i_fop = &assoofs_file_operations;
    
    //Funcion auxiliar para asignarle un bloque al nuevo inodo
    aux = assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);
    
    //Control de errores
    if(aux < 0){
    	printk(KERN_ERR "Simplefs no tiene un bloque libre.\n");
        return -1;
    }
    
    //Incluimos campo i_private
    inode->i_private = inode_info;
    
    //Funcion auxiliar para guardar la informacion persistente del nuevo inodo en disco
    assoofs_add_inode_info(sb, inode_info);
    
    /** 2. Modifico el contenido del directorio padre **/
    parent_inode_info = dir->i_private;
    
    bh = sb_bread(sb, parent_inode_info->data_block_number);
    
    dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;
    dir_contents += parent_inode_info->dir_children_count;
    
    // inode_info es la informacion persistente del inodo creado en el paso 2.
    dir_contents->inode_no = inode_info->inode_no;
    
    strcpy(dir_contents->filename, dentry->d_name.name);
    
    //Marco como sucio
    mark_buffer_dirty(bh);
    //Sincronizo
    sync_dirty_buffer(bh);
    //Libero
    brelse(bh);
    
    /** 3. Actualizo la info del inodo padre, indicandole que tiene 1 archivo nuevo **/
    parent_inode_info->dir_children_count++;
    
    //Funcion auxiliar para actualizar la info
    assoofs_save_inode_info(sb, parent_inode_info);
    
    inode_init_owner(inode, dir, mode);
    d_add(dentry, inode);
 
    printk(KERN_INFO "********** Fin llamada a Create **********\n");
    
    return 0;
}

/******************************* Creacion de directorios MKDIR *******************************/
static int assoofs_mkdir(struct inode *dir , struct dentry *dentry, umode_t mode) {

    //DECLARACIONES
 	int aux;
   	uint64_t count;
    struct inode *inode;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct assoofs_dir_record_entry *dir_contents;
    struct super_block *sb; 
    struct buffer_head *bh; 
        
    printk(KERN_INFO "\n********** Llamada a Mkdir **********\n");
    
    /** 1. Creo nuevo inodo **/
    // obtengo un puntero al superbloque desde dir
    sb = dir->i_sb;
    
    // obtengo el número de inodos de la informacion persistente del superbloque
    count = ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count;
    
    //Compruebo que el valor de count no es superior al numero maximo de objetos soportados
    if(count >= ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED){
        printk(KERN_ERR"ERROR, El archivo esta completo.\n" );
        return -12;
    }
    
    //Nuevo inodo
    inode = new_inode(sb);
    
    // Asigno numero al nuevo inodo a partir de count
    inode->i_ino = (count + ASSOOFS_START_INO - ASSOOFS_RESERVED_INODES + 1);
    
    //Guardo el superbloque en el inodo
    inode->i_sb = sb;
    
    inode->i_op = &assoofs_inode_ops;
    
    // fechas.
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    
    inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
    
    // El segundo mode me llega como argumento
    inode_info->mode = S_IFDIR | mode;
    inode_info->inode_no = inode->i_ino; /** hhhhhh **/

    printk(KERN_INFO "      MKDIR - Solicitud de creación de nuevo directorio %s.", dentry -> d_name.name);
    
	//Para las operaciones sobre directorios
	inode_info->dir_children_count = 0;
    inode->i_fop = &assoofs_dir_operations;

    //Funcion auxiliar para asignarle un bloque al nuevo inodo
    aux = assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);
    
    //Control de errores
    if(aux < 0){
        printk(KERN_ERR "Simplefs no tiene un bloque libre.\n");
        return -1;
    }
    
    //Incluimos campo i_private
    inode->i_private = inode_info;
    
    //Funcion auxiliar para guardar la informacion persistente del nuevo inodo en disco
    assoofs_add_inode_info(sb, inode_info);
    
    /** 2. Modifico el contenido del directorio padre **/
    parent_inode_info = dir->i_private;
    
    bh = sb_bread(sb, parent_inode_info->data_block_number);
    
    dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;
    dir_contents += parent_inode_info->dir_children_count;
    
    // inode_info es la informacion persistente del inodo creado en el paso 2.
    dir_contents->inode_no = inode_info->inode_no;
    
    strcpy(dir_contents->filename, dentry->d_name.name);
    
    //Marco como sucio
    mark_buffer_dirty(bh);
    //Sincronizo
    sync_dirty_buffer(bh);
    //Libero
    brelse(bh);
    
    /** 3. Actualizo la info del inodo padre, indicandole que tiene 1 archivo nuevo **/
    parent_inode_info->dir_children_count++;
    
    //Funcion auxiliar para actualizar la info
    assoofs_save_inode_info(sb, parent_inode_info);
    
    inode_init_owner(inode, dir, inode_info->mode);
    d_add(dentry, inode);
    
    printk(KERN_INFO "********** Fin llamada a Mkdir **********\n");
    return 0;
}


/**********************************************************************************************
 *                               Operaciones sobre el superbloque                             *
 **********************************************************************************************/

/***************************** Inicialización del superbloque *****************************/
int assoofs_fill_super(struct super_block *sb, void *data, int silent) {
    
    //DECLARACIONES
    struct buffer_head *bh;
    struct assoofs_super_block_info *assoofs_sb;
    struct inode *root_inode; //Declaro nuevo inodo
    
    printk(KERN_INFO "--------------------------------------------------");
    printk(KERN_INFO "assoofs_fill_super Solicitado\n");
    
    /** 1.- Leo la información persistente del superbloque del dispositivo de bloques **/
    //La funcion assoofs_fill_super recibe el argumento sb ** ANEXO C **
    bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
    
    assoofs_sb = (struct assoofs_super_block_info *)bh->b_data;
 
    /** 2.- Compruebo los parámetros del superbloque **/
    if(assoofs_sb->magic != ASSOOFS_MAGIC){
        printk(KERN_ERR "ERROR, El sistema de archivos que quieres montar no es de tipo ASSOOFS.\n" );
        //Libero Recursos
        brelse(bh);
        return -1;
    }
    else{
        printk(KERN_INFO "Numero magico correcto. El numero magico es %llu.\n", assoofs_sb->magic);
    }
    
    if(assoofs_sb->block_size != ASSOOFS_DEFAULT_BLOCK_SIZE){
        printk(KERN_ERR"ERROR, ASSOOFS formateado con tamaño de bloque erroneo.\n" );
        //Libero Recursos
        brelse(bh);
        return -1;
    }
    else{
        printk(KERN_INFO "El Sistema de Archivos ASSOOFS version %llu formateado con un tamaño de bloque correcto. El tamaño de bloque es %llu.\n", assoofs_sb->version, assoofs_sb->block_size);
    }
    
    /** 3.- Escribo la info persist leída del dispos de bloq en el superbloq sb, incluído el campo s_op con ops soportadas **/  
    //Asigno el numero magico al superbloque recibido por parametro  
    sb->s_magic = ASSOOFS_MAGIC; 
    sb->s_maxbytes = ASSOOFS_DEFAULT_BLOCK_SIZE;
    sb->s_op = &assoofs_sops;
    //Para evitar acceder al bloque 0 constantemente, guardo la info leida en el parametro s_fs_info
    sb->s_fs_info = assoofs_sb;
    
    /** 4.- Creo el inodo raíz y le asigno operaciones sobre inodos (i_op) y sobre dir (i_fop) **/
    
    /** Creo inodos ANEXO E **/
    
    //Creo nuevo inodo
    root_inode = new_inode(sb);
    
    //Lo inicializo
    inode_init_owner(root_inode, NULL, S_IFDIR);
    
    // numero de inodo
    root_inode->i_ino = ASSOOFS_ROOTDIR_INODE_NUMBER;
    // puntero al superbloque
    root_inode->i_sb = sb;
    // direccion de una variable de tipo struct inode_operations previamente declarada
    root_inode->i_op = &assoofs_inode_ops;
    // direccion de una variable de tipo struct dir_operations previamente declarada
    root_inode->i_fop = &assoofs_dir_operations;
    // fechas.
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = current_time(root_inode);
    
    // Informacion persistente del inodo
    root_inode->i_private = assoofs_get_inode_info(sb, ASSOOFS_ROOTDIR_INODE_NUMBER);
    
    //Introduzco el nuevo inodo del arbol //el nuevo inodo es inodo raiz
    sb->s_root = d_make_root(root_inode);

   	//Si sb no entra en el inodo, devuelvo error, libero la memoria y return 
   	if(!sb->s_root){
   		return -12;
   	}
    printk(KERN_INFO "assoofs_fill_super Completado Satisfactoriamente\n");
    printk(KERN_INFO "--------------------------------------------------");
   
   	return 0;
}


/**********************************************************************************************
 *                                   Funciones Auxiliares                                      *
 **********************************************************************************************/

/******************************* Funcion assoofs_get_inode_info ********************************/
//Funcion auxiliar que me permite obtener la informacion persistente del inodo numero inode_no del superbloque sb
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no){
    
    //DECLARACIONES
    struct assoofs_inode_info *inode_info = NULL;
    struct buffer_head *bh;
    struct assoofs_super_block_info *afs_sb = sb->s_fs_info;
    struct assoofs_inode_info *buffer = NULL;
    int i;
    
    /** 1. Accedo al disco para leer el bloque que contiene el almacen de inodos **/
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *)bh->b_data;
    
    /** 2. Recorro el almacen de inodos en busca del inodo inode_no **/
    for (i = 0; i < afs_sb->inodes_count; i++) {
        if (inode_info->inode_no == inode_no) {
            buffer = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
            memcpy(buffer, inode_info, sizeof(*buffer));
            break;
        }
        inode_info++;
    }
    
    /** 3. Libero recursos y devuelvo la informacion del inodo inode_no si estaba en el almacen **/
    brelse(bh);
    
    return buffer;
}

/******************************* Funcion Look_up (2.3.4) *******************************/
//Funcion que busca la entrada (struct dentry) con el nombre correcto (child dentry->d name.name) en el directorio padre (parent inode)
//La utilizo para recorrer y mantener el arbol de inodos
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    
    //DECLARACIONES
    struct assoofs_inode_info *parent_info = parent_inode->i_private;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;
    
    printk(KERN_INFO "\n********** Llamada a Lookup **********\n");
    
    /** 1. Accedo al bloque de disco apuntado por parent inode **/
    bh = sb_bread(sb, parent_info->data_block_number);
    
    /** 2. Recorro el contenido del dir buscando la entrada (el nombre corresponde con buscado) **/  
    record = (struct assoofs_dir_record_entry *)bh->b_data;
    
    for (i = 0; i < parent_info->dir_children_count; i++) {
        //Si localizo la entrada, entonces tengo que construir el inodo correspondiente.
        if (!strcmp(record->filename, child_dentry->d_name.name)) {
            // Funcion auxiliar que obtiene la info de un inodo a partir de su numero de inodo.
            struct inode *inode = assoofs_get_inode(sb, record->inode_no);
            inode_init_owner(inode, parent_inode, ((struct assoofs_inode_info *)inode->i_private)->mode);
            d_add(child_dentry, inode);
            //Libero
            brelse(bh);
            printk(KERN_INFO "      LOOKUP - Archivo encontrado %s.\n", record->filename);
            return NULL;
        }
        record++;
    }
    
    printk(KERN_INFO "      LOOKUP - Archivo %s no encontrado.\n", child_dentry->d_name.name); /*** HHHH **/
    printk(KERN_INFO "********** Fin llamada a Lookup **********\n");
    
    return NULL;
}

/*************************** Funcion assoofs_get_inode (2.3.4) *****************************/
static struct inode *assoofs_get_inode(struct super_block *sb, int ino){
    
    //DECLARACIONES
    struct inode *inode; //Creo el nuevo inodo
    struct assoofs_inode_info *inode_info;

    /** 1. Obtengo la informacion persistente del inodo ino **/
    inode_info = assoofs_get_inode_info(sb, ino);
    
    /** 2. Creo una nueva variable de tipo struct inode y la inicializo con funcion new_inode **/
    //Creo nuevo inodo
    inode = new_inode(sb);
    
    inode->i_ino = ino;
    inode->i_sb = sb;
    inode->i_op = &assoofs_inode_ops;  
    
    //Antes de asignar valor al campo i_fop debo saber si el inodo que busco es un fich o un dir
    if (S_ISDIR(inode_info->mode))
        inode->i_fop = &assoofs_dir_operations;
    else if (S_ISREG(inode_info->mode))
        inode->i_fop = &assoofs_file_operations;
    else
        printk(KERN_ERR "Tipo de inodo desconocido. No es ni directorio ni fichero.\n");
    
    // Asigno el valor CURRENT TIME a los campos i_atime, i_mtime y i_ctime del nuevo inodo.
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    
    // Guardo la informacion persistente del inodo obtenida en el paso 1
    inode->i_private = assoofs_get_inode_info(sb, ino);
    
    return inode;
}

/************************ Funcion assoofs_sb_get_a_freeblock (2.3.4) ***************************/
int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block){
    
    //DECLARACIONES
    struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
    int i = 0;

    printk(KERN_INFO "\n********** Llamada a Get A Freeblock **********\n");
    
    /** 1. Compruebo que no alcanza el numeo maximo de objetos en un sistema assoofs **/
    // Cuando aparece el 1º bit 1 en free_block dejo de recorrer el mapa de bits
    for (i = ASSOOFS_RESERVED_INODES; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++)
        if (assoofs_sb->free_blocks & (1 << i))
            break; //i tiene la posicion del primer bloque libre
    
       
    //Cuando ya no queda espacio ni bloques libres
    if((i + 1) == ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED){
    	printk(KERN_ERR "Espacio en el sistema agotado.");
    	return -28;
    }
    
    // Escribo el valor de i en la direccion de memoria indicada como 2º argumento en la funcion
    *block = i;
    
    /** 2. Actualizo el valor de free_blocks **/
    assoofs_sb->free_blocks &= ~(1 << i);
    
    //Guardo los cambios en el superbloque llamando a una funcion auxiliar
    assoofs_save_sb_info(sb);
 
    printk(KERN_INFO "********** Fin llamada a Get A Freeblock **********\n");
    return 0;
}

/*************************** Funcion assoofs_save_sb_info (2.3.4) ******************************/
void assoofs_save_sb_info(struct super_block *vsb){
    
    //DECLARACIONES
    struct buffer_head *bh;
    struct assoofs_super_block *sb = vsb->s_fs_info; //Info persistente del superbloque en memoria
    
    bh = sb_bread(vsb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
    // Sobreescribo los datos de disco con la informacion en memoria
    bh->b_data = (char *)sb;
    
    //Marco el bloque como sucio
    mark_buffer_dirty(bh);
    //Sincronizo
    sync_dirty_buffer(bh);
    //Libero
    brelse(bh); /** hhhh **/

}

/*************************** Funcion assoofs_add_inode_info (2.3.4) *****************************/
void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode){
    
    //DECLARACIONES
    struct buffer_head *bh;
    struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
    struct assoofs_inode_info *inode_info; 
    
    printk(KERN_INFO "\n********** Llamada a Add Inode Info **********\n");
    
    printk(KERN_INFO "      ADD INODE - Intentando añadir nuevo inodo de %llu bytes.\n", inode->file_size);
           
    //Leo de disco el bloque que contiene el almacen de inodos
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    
    //Obtengo puntero al final del almacen y escribo nuevo valor final
    inode_info = (struct assoofs_inode_info *)bh->b_data;
    inode_info += assoofs_sb->inodes_count;
    memcpy(inode_info, inode, sizeof(struct assoofs_inode_info));
    
    //Marco el bloque como sucio
    mark_buffer_dirty(bh);
    //Sincronizo
    sync_dirty_buffer(bh);
    
    //Libero
    brelse(bh);
    
    //Actualizo el contador de la informacion persistente del superbloque
    assoofs_sb->inodes_count++;
    //Guardo los cambios
    assoofs_save_sb_info(sb);
    
    printk(KERN_INFO "********** Fin llamada a Add Inode Info **********\n");
}

/************************** Funcion assoofs_save_inode_info (2.3.4) ****************************/
int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info){
    
    //DECLARACIONES
    struct buffer_head *bh;
    struct assoofs_inode_info *inode_pos;

    //Obtengo de disco el almacen de inodos
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    
    //Busco los datos de inode_info en el almacen
    inode_pos = assoofs_search_inode_info(sb, (struct assoofs_inode_info *)bh->b_data, inode_info);
    
    if(inode_pos){
    	//Actualizo el inodo
    	memcpy(inode_pos, inode_info, sizeof(*inode_pos));
    	//Marco el bloque como sucio
    	mark_buffer_dirty(bh);
    	//Sincronizo
    	sync_dirty_buffer(bh);
    }
    else{
    	printk(KERN_ERR "No se puede guardar el nuevo tamaño en el inodo.\n");
    }
    
    //Libero Recursos
    brelse(bh);
    
    //Si todo va bien devuelvo 0
    return 0;
}

/************************** Funcion assoofs_search_inode_info (2.3.4) ****************************/
struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search){
    
    //DECLARACIONES
    uint64_t count = 0;
    
    //Recorro el almacen de inodos desde start (inicio del almacen) hasta encontrar *search
    while (start->inode_no != search->inode_no && count < ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count) {
        count++;
        start++; 
    }
    if (start->inode_no == search->inode_no)
        return start;
    else
        return NULL;
}


/**********************************************************************************************
 *                              Montaje de dispositivos assoofs                               *
 **********************************************************************************************/

static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    //DECLARACIONES
    struct dentry *ret;
    
    printk(KERN_INFO "\n********** Llamada a Mount **********\n");
    printk(KERN_INFO "      MOUNT - El Sistema de Ficheros ASSOOFS se montara em %s.\n", dev_name);
    
    ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);

    // Control de errores a partir del valor de ret. En este caso se puede utilizar la macro IS_ERR: if (IS_ERR(ret)) ...
    if(IS_ERR(ret)) printk(KERN_ERR "Error al intentar montar ASSOOFS en %s.\n", dev_name);
    else printk(KERN_INFO "      MOUNT - El Sistema de Ficheros ASSOOFS montado correctamente en %s.\n", dev_name);
    
    return ret;
}


/**********************************************************************************************
 *                                 assoofs file system type                                   *
 **********************************************************************************************/

static struct file_system_type assoofs_type = {
    .owner   = THIS_MODULE,
    .name    = "assoofs",
    .mount   = assoofs_mount,
    .kill_sb = kill_litter_super,
};

extern int register_filesystem(struct file_system_type *);
extern int unregister_filesystem(struct file_system_type *);

static int __init assoofs_init(void) {

    //DECLARACIONES
    int ret = register_filesystem(&assoofs_type);  
        
    // Control de errores a partir del valor de ret
     if(likely(ret == 0)) printk(KERN_INFO "Sistema de Archivos ASSOOFS registrado con éxito.\n");
    else printk(KERN_ERR "Fallo en el montaje del sistema de archivos al registrar assoofs. ERROR [%d].\n", ret);
    
    //PARTE OPCIONAL
    //Inicio la cache de inodos
    //assoofs_inode_cache = kmem_cache_create("assoofs_inode_cache", sizeof(struct assoofs_inode_info), 0, (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);  

    return ret;
}

static void __exit assoofs_exit(void) {

    //DECLARACIONES
    int ret = unregister_filesystem(&assoofs_type);
    
    // Control de errores a partir del valor de ret
   if(likely(ret == 0)) printk(KERN_INFO "Todo correcto, el sistema de archivos se ha desmontado.\n");
    else printk(KERN_ERR "Fallo en el desmontaje del sistema de archivos (Elminimacion de registro). ERROR [%d].\n", ret);
    
    //PARTE OPCIONAL
    //Libero la cache cuando descargue el modulo del kernel
    //kmem_cache_destroy(assoofs_inode_cache);
    
}

module_init(assoofs_init);
module_exit(assoofs_exit);

