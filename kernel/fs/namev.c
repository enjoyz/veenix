#include "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{

       /* NOT_YET_IMPLEMENTED("VFS: lookup");
        return 0;*/
    int tmp;
    if((tmp = (dir->vn_ops->lookup(dir,name,len,result))) >= 0)
        return tmp;
    return -ENOTDIR;
    

}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
    /* NOT_YET_IMPLEMENTED("VFS: dir_namev");
        return 0;*/
    /**** STRING MANIPULATIONS ******/
    char *pathname2 = (char*)kmalloc(sizeof(pathname));
    strcpy(pathname2,pathname);
    char *tmp;
    tmp = pathname2;
    int i=0,count = 0;
    int ret_val;
    

    while(tmp[i] != '\0')
    {
        if(tmp[i] == '/')
            count++;
        i++;
    }
    tmp = strtok(pathname,"/");
    
    if(pathname[0] == '/')
        ret_val=lookup(vfs_root_vn, tmp, strlen(tmp), res_vnode);
    else
        ret_val=lookup(curproc->p_cwd, tmp, strlen(tmp), res_vnode);

    if(ret_val<0)
        return ret_val;

    if(count<2)
    {
        strcpy(*name,tmp);
        *namelen=strlen(tmp);
        return ret_val;
    }

    
    vnode_t *temp_res_vnode=kmalloc(sizeof(vnode_t));

    vput(res_vnode);
    while(count-2 > 0)
    {
        tmp=strtok(NULL,"/");
        temp_res_vnode=*res_vnode;

        ret_val=lookup(temp_res_vnode, tmp, strlen(tmp), res_vnode);
        if(ret_val<0)
            return ret_val;
        count--;
        if(count-2>0 )
            vput(res_vnode);
    }
    tmp=strtok(NULL,"/");
    strcpy(*name,tmp);
    *namelen=strlen(tmp);
    return ret_val;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
       /* NOT_YET_IMPLEMENTED("VFS: open_namev");
        return 0;*/
        char *Name;
        int Path_Len = strlen(pathname);
        int ret_val = dir_namev(pathname,&Path_Len,&Name, base, res_vnode);
        if(ret_val < 0)
            return ret_val;
        ret_val = lookup(res_vnode,Name,strlen(Name),res_vnode);
        int  creat_ret;
        if(ret_val < 0 && flag == O_CREAT){
            vnode_t *temp_res_vnode = kmalloc(sizeof(vnode_t));
            temp_res_vnode = *res_vnode;
            creat_ret=res_vnode->vn_ops->create(temp_res_vnode, Name,strlen(Name), res_vnode);
        }
        else if(ret_val > 0) 
        {
            vput(*res_vnode);
            return ret_val;
        }
        return creat_ret;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
