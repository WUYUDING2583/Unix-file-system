#include<iostream>
#include<fstream>
#include <fcntl.h>//根据文件描述词操作文件的特性
#include<time.h>
#include<conio.h>
#include<string>
#include<map>
#include <stdlib.h>
#include<iomanip>
#include<vector>
using namespace std;

const int BLOCKNUM = 512;//数据区卷盘块数
const int SPARESTACKNUM = 50;//空闲盘块栈长度
const int INODETOTALNUM = 256;//inode总数
const int INODENUM = 64;//inode盘块数
const int BLOCKSIZE = 512;//块大小
const int INODESIZE = 128;//inode大小
const int INODEMAPSTART = BLOCKSIZE;//inodemap开始位置
const int INODEMAPSIZE = sizeof(int)*INODETOTALNUM;//索引节点表长度
const int INODESTART = INODEMAPSTART + INODEMAPSIZE;//索引节点开始位置
const int DATASTART = INODESTART + INODETOTALNUM * INODESIZE;//数据区开始位置
const int ADDRSIZE = 6;//inode文件地址项
const enum FILEMODE { DIR = 'd', FIL = '-', LINK = 'l' };//目录：d 普通文件：- 链接文件：l
const int DIRMAXFILENUM = 20;//目录最大文件数
const int MAXUSERNUM = 10; //系统支持最大的用户数量
const int MAXNAMELENGTH = 20;//系统支持最大文件名长度
const enum COMMAND {
	evNotDefined, ls, chmod, chown, chgrp, pwd, cd, mkdir, rmdir
	, umask, help, mv, cp, rm, ln, cat, passwd, vi, exi, detail
};
const enum AUTHORITY { READ = 'r', WRITE = 'w', EXE = 'x', NONE = '-' };
const int ADDRBLOCKNUM = 20;//addr[4]能存放的最大盘块数



//定义超级块
struct SuperBlock
{
	int size;				//文件系统的盘块数
	int inodeSize;			//文件系统的inode块数
	int spareBlock[SPARESTACKNUM + 1];	//空闲盘块号栈，即用于记录当前可用的空闲盘块编号的栈,用于成组连接。
	int spareBlockNum;			//空闲盘块数，用于记录整个文件系统中未被分配的盘块个数。
	int spareInodeNum;			//空闲i结点个数，用于记录整个文件系统中未被分配的节点个数。
	char lastLogin[64];			//文件系统上次登陆时间

};

//定义索引节点
struct inode
{
	int inodeNo;            //inode号.
	long int fileSize; //文件大小
	int count;		//普通文件文件链接计数
	char lastTime[64];//最后修改时间
	int dircount;	//目录文件第一级子目录数
	int addr[ADDRSIZE];//文件存储地址
	int ownerId;	//文件拥有者
	int groupId;	//文件所属组
	char mode;		//文件类型
	char authority[9];	//权限
};

//定义目录结构
struct direct
{
	char fileName[DIRMAXFILENUM][MAXNAMELENGTH];		//文件名或目录名
	int	inodeID[DIRMAXFILENUM];			//文件索引结点号
};

//定义用户结构
struct user
{
	int userId[MAXUSERNUM]; //用户id
	char userName[MAXUSERNUM][MAXNAMELENGTH];//用户名
	char userPsw[MAXUSERNUM][MAXNAMELENGTH]; //用户密码
	int userGroup[MAXUSERNUM]; //用户所属组
};

//struct document
//{
//	char fileName[MAXNAMELENGTH];
//	string content;
//};

//定义全局变量
FILE* file;
SuperBlock superblock;//全局超级块
int inodeMap[INODETOTALNUM];//索引节点表
direct dir;//全局当前目录
inode currentInode;//全局当前所在文件的索引节点
inode rootInode;//根目录索引节点
user accounts;
int userId;//全局用户id
int userGroup;//全局用户组
vector<string> path;
map<string, COMMAND> s_mapStringValues;
int UMASK[3] = { 0,2,2 };//默认umask码

//获取系统当前时间
char * getTime()
{
	time_t timep;
	time(&timep);
	char tmp[64];
	strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&timep));
	return tmp;
}

//更新索引节点表
int UpdateInodeMap()
{
	//创建新的inode
	int no;
	for (int i = 0; i < INODETOTALNUM; i++)
	{
		if (inodeMap[i] != 1)
		{
			no = i;
			inodeMap[i] = 1;//更新索引节点表
			break;
		}
	}
	//将索引节点表写入
	fseek(file, INODEMAPSTART, SEEK_SET);
	fwrite(&inodeMap, INODEMAPSIZE, 1, file);
	return no;
}

//将umask码转为二进制数组
int * DecToBin(int t)
{
	int * um = new int[3];
	int j = 1;
	int a = t;
	while (a / 2 != 0)
	{
		int temp = a % 2;
		um[3 - j] = temp;
		j++;
		a /= 2;
	}
	um[0] = a % 2;
	return um;
}

//密码加密
string Encryption()
{
	char userPsw[MAXNAMELENGTH], ch;
	int i = 0;
	string psw;
	while ((ch = _getch()) != '\r')
	{
		if (ch == '\b' && i > 0)
		{
			cout << "\b \b";
			--i;
		}
		else if (ch != '\b')
		{
			userPsw[i++] = ch;
			cout << "*";
		}
	}
	for (int j = 0; j < i; j++)
		psw += userPsw[j];
	cout << endl;
	return psw;
}

//通过umask码设置文件权限
void setAuthority(char type, char * authority)
{
	int * au = new int[9];
	int mu[3];
	if (type == 'd')
	{
		for (int i = 0; i < 3; i++)
		{
			mu[i] = 7 - UMASK[i];
		}

	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			mu[i] = 6 - UMASK[i];
		}
	}
	for (int i = 0; i < 3; i++)
	{
		int* t = new int[3];
		t = DecToBin(mu[i]);
		for (int j = 0; j < 3; j++)
		{
			au[i * 3 + j] = t[j];
		}
	}
	for (int i = 0; i < 3; i++)
	{
		if (au[i * 3] == 1)
			authority[i * 3] = AUTHORITY::READ;
		else
			authority[i * 3] = AUTHORITY::NONE;
		if (au[i * 3 + 1] == 1)
			authority[i * 3 + 1] = AUTHORITY::WRITE;
		else
			authority[i * 3 + 1] = AUTHORITY::NONE;
		if (au[i * 3 + 2] == 1)
			authority[i * 3 + 2] = AUTHORITY::EXE;
		else
			authority[i * 3 + 2] = AUTHORITY::NONE;
	}
}

//新建文件卷
void init()
{
	//为当前目录新建文件作为文件卷
	FILE* file = fopen("yuyi.txt", "wb+");
	if (!file)
	{
		cout << "文件卷建立失败......." << endl;
	}
	else
	{
		cout << "文件卷创建成功" << endl;
	}
	//初始化超级块
	SuperBlock superblock;
	superblock.size = BLOCKNUM + 1 + 64;//1表示超级块,64为索引节点块数
	superblock.spareBlockNum = BLOCKNUM - 2;
	superblock.inodeSize = INODESIZE;
	superblock.spareInodeNum = INODETOTALNUM - 2;
	fwrite(&superblock, sizeof(SuperBlock), 1, file);

	//初始化索引节点表
	int inodeMap[INODETOTALNUM];
	memset(inodeMap, 0, sizeof(int)*INODETOTALNUM);
	inodeMap[0] = 1;//表示第一二各索引节点已用，指向根目录和用户文件
	inodeMap[1] = 1;
	fseek(file, INODEMAPSTART, SEEK_SET);
	fwrite(&inodeMap, INODEMAPSIZE, 1, file);

	//成组连接
	int spareBlockTemp[SPARESTACKNUM + 1];
	for (int i = 0; i < BLOCKNUM / SPARESTACKNUM; i++)
	{
		for (int j = 0; j < SPARESTACKNUM; j++)
		{
			spareBlockTemp[j] = (SPARESTACKNUM - 1) + (i * SPARESTACKNUM) - j;
			if (i == 0)
			{//将第一组空闲盘块的信息存入超级块中
				superblock.spareBlock[j] = spareBlockTemp[j];
			}
		}
		if (i != 0)
		{
			spareBlockTemp[SPARESTACKNUM] = SPARESTACKNUM - 1;
			fseek(file, DATASTART + (SPARESTACKNUM - 1 + (i - 1) * SPARESTACKNUM)*BLOCKSIZE, SEEK_SET);
			fwrite(&spareBlockTemp, sizeof(int) * (SPARESTACKNUM + 1), 1, file);
		}
	}
	superblock.spareBlock[SPARESTACKNUM] = SPARESTACKNUM - 1 - 2;
	fseek(file, 0, SEEK_SET);
	fwrite(&superblock, sizeof(SuperBlock), 1, file);

	//最后一组剩余盘块
	memset(&spareBlockTemp, 0, sizeof(spareBlockTemp));
	for (int i = 0; i < BLOCKNUM % SPARESTACKNUM; i++)
	{
		spareBlockTemp[i] = (BLOCKNUM - 1) - i;
	}
	spareBlockTemp[0] = 0;
	spareBlockTemp[SPARESTACKNUM] = BLOCKNUM % SPARESTACKNUM;
	fseek(file, DATASTART + (SPARESTACKNUM + (BLOCKNUM / SPARESTACKNUM) * SPARESTACKNUM)*BLOCKSIZE
		, SEEK_SET);
	fwrite(&spareBlockTemp, sizeof(int) * (SPARESTACKNUM + 1), 1, file);

	//创建根目录占据第一个inode节点
	inode irootDir;
	irootDir.inodeNo = 0;
	irootDir.fileSize = 0;
	irootDir.dircount = 2;
	irootDir.mode = FILEMODE::DIR;
	memset(irootDir.addr, -1, sizeof(int)*ADDRSIZE);//初始化目录地址
	irootDir.addr[0] = 0;
	irootDir.groupId = 0;
	irootDir.ownerId = 0;
	for (int i = 0; i < 3; i++)
	{
		irootDir.authority[i * 3] = (char)AUTHORITY::READ;
		irootDir.authority[i * 3 + 1] = (char)AUTHORITY::NONE;
		irootDir.authority[i * 3 + 2] = (char)AUTHORITY::NONE;
	}
	strcpy(irootDir.lastTime, getTime());
	fseek(file, INODESTART + INODESIZE * irootDir.inodeNo, SEEK_SET);
	fwrite(&irootDir, sizeof(inode), 1, file);

	//创建目录文件，占据第一个数据块
	direct drootDir;
	memset(drootDir.fileName, 0, sizeof(char) * 20 * MAXNAMELENGTH);
	memset(drootDir.inodeID, -1, sizeof(int)*DIRMAXFILENUM);
	strcpy(drootDir.fileName[0], ".");
	drootDir.inodeID[0] = 0;
	strcpy(drootDir.fileName[1], "..");
	drootDir.inodeID[1] = 0;
	fseek(file, DATASTART + BLOCKSIZE * irootDir.addr[0], SEEK_SET);
	fwrite(&drootDir, sizeof(direct), 1, file);

	//创建用户文件
	//创建用户文件inode，占据第二个索引节点
	inode userInode;
	userInode.fileSize = sizeof(user);
	userInode.count = 1;
	userInode.inodeNo = 1;
	memset(userInode.addr, -1, sizeof(int)*ADDRSIZE);
	userInode.addr[0] = 1;
	userInode.mode = FILEMODE::FIL;
	for (int i = 0; i < 3; i++)
	{
		userInode.authority[i * 3] = AUTHORITY::READ;
		userInode.authority[i * 3 + 1] = AUTHORITY::NONE;
		userInode.authority[i * 3 + 2] = AUTHORITY::NONE;
	}
	userInode.groupId = 0;
	userInode.ownerId = 0;
	strcpy(userInode.lastTime, getTime());
	fseek(file, INODESTART + INODESIZE * userInode.inodeNo, SEEK_SET);
	fwrite(&userInode, sizeof(inode), 1, file);

	//创建账户
	user account;
	memset(account.userName, -1, sizeof(char)*MAXUSERNUM*MAXNAMELENGTH);
	memset(account.userPsw, -1, sizeof(char)*MAXUSERNUM*MAXNAMELENGTH);
	strcpy(account.userName[0], "yuyi");
	strcpy(account.userPsw[0], "yuyi");
	strcpy(account.userName[1], "guest");
	strcpy(account.userPsw[1], "guest");
	strcpy(account.userName[2], "zhaoyu");
	strcpy(account.userPsw[2], "zhaoyu");
	for (int i = 0; i < MAXUSERNUM; i++)
	{
		account.userId[i] = i;
	}
	//设置用户组，管理员为0， 普通用户为其他数字
	account.userGroup[0] = 0;
	account.userGroup[1] = 1;
	account.userGroup[2] = 0;
	//占据第二个数据块
	fseek(file, DATASTART + BLOCKSIZE * userInode.addr[0], SEEK_SET);
	fwrite(&account, sizeof(user), 1, file);

	fclose(file);
}

//读取超级块信息
void Mount()
{
	//打开文件卷
	file = fopen("yuyi.txt", "rb+");
	if (file == NULL)
	{
		cout << "文件卷未找到，正在新建文件卷......" << endl;
		init();
	}

	//读取超级块内容
	fread(&superblock, sizeof(SuperBlock), 1, file);

	//读取索引节点表
	fseek(file, INODEMAPSTART, SEEK_SET);
	fread(&inodeMap, sizeof(int)*INODETOTALNUM, 1, file);

	//读取根目录
	fseek(file, DATASTART, SEEK_SET);
	fread(&dir, sizeof(direct), 1, file);

	//读取用户信息
	fseek(file, DATASTART + BLOCKSIZE, SEEK_SET);
	fread(&accounts, sizeof(user), 1, file);

	//读取根目录的inode信息
	fseek(file, INODESTART, SEEK_SET);
	fread(&currentInode, sizeof(inode), 1, file);
	rootInode = currentInode;

	path.push_back("home");

}

//登录
bool Login(string userName, string userPsw)
{
	if (userName.length() == 0 || userPsw.length() == 0)
	{
		cout << "用户名或密码不能为空" << endl;
		return false;
	}
	else if (userName.length() > MAXNAMELENGTH || userPsw.length() > MAXNAMELENGTH)
	{
		cout << "用户名或密码不能超过" << MAXNAMELENGTH << "位" << endl;
		return false;
	}
	else
	{
		for (int i = 0; i < MAXUSERNUM; i++)
		{
			if (strcmp(accounts.userName[i], userName.c_str()) == 0)
			{
				if (strcmp(accounts.userPsw[i], userPsw.c_str()) == 0)
				{
					userId = i;
					userGroup = accounts.userGroup[i];
					return true;
				}
			}
		}
		cout << "用户名或密码错误" << endl;
		return false;
	}
}

//输出前缀（包含当前文件）
void Prefix()
{
	string group;
	if (userGroup == 0)
		group = "root";
	else
		group = accounts.userName[userId];
	string pa = "～";
	for (int i = 1; i < path.size(); i++)
	{
		pa += "/" + path[i];
	}
	cout << group << "@" << accounts.userName[userId] << ":" << pa << "$ ";
}

void initialCommand()
{
	s_mapStringValues["ls"] = ls;
	s_mapStringValues["detail"] = detail;
	s_mapStringValues["chmod"] = chmod;
	s_mapStringValues["chown"] = chown;
	s_mapStringValues["chgrp"] = chgrp;
	s_mapStringValues["pwd"] = pwd;
	s_mapStringValues["cd"] = cd;
	s_mapStringValues["mkdir"] = mkdir;
	s_mapStringValues["rmdir"] = rmdir;
	s_mapStringValues["mv"] = mv;
	s_mapStringValues["cp"] = cp;
	s_mapStringValues["ln"] = ln;
	s_mapStringValues["cat"] = cat;
	s_mapStringValues["passwd"] = passwd;
	s_mapStringValues["vi"] = vi;
	s_mapStringValues["help"] = help;
	s_mapStringValues["umask"] = umask;
	s_mapStringValues["exit"] = exi;
	s_mapStringValues["rm"] = rm;
}

void Help()
{
	cout << setw(3) << " " << setiosflags(ios::left) << setw(10) << "help" << "列出所有命令及其作用" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "ls" << "显示文件目录" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "chmod" << "改变文件权限" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "chown" << "改变文件拥有者" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "chgrp" << "改变文件所属组" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "pwd" << "显示当前目录" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "cd" << "改变当前目录" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "mkdir" << "创建子目录" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "rmdir" << "删除子目录" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "mv" << "改变文件名" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "umask" << "文件创建屏蔽码" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "cp" << "文件拷贝" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "ln" << "建立文件联接" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "cat" << "连接显示文件内容" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "passwd" << "修改用户口令" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "vi" << "编辑文本" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "detail" << "显示超级块信息" << endl
		<< setw(3) << " " << setiosflags(ios::left) << setw(10) << "exit" << "退出系统" << endl;
}

void Exit()
{
	fclose(file);
	exit(0);
}

//检验是否有重名文件
bool CheckRepetedName(string name, direct direct)
{

	char dirName[MAXNAMELENGTH];
	strcpy(dirName, name.c_str());
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strcmp(dirName, direct.fileName[i]) == 0)
			return false;
	}
	return true;
}

//检查写权限
bool CheckWrite(inode inode, string type)
{

	if (userGroup != 0)
	{
		if (type.compare("sub") == 0)//写操作在子文件上
		{
			if (userGroup != inode.groupId)
			{
				if (inode.authority[7] != AUTHORITY::WRITE)
				{
					return false;
				}
			}
			else
			{
				if (userId != inode.ownerId)
				{
					if (inode.authority[4] != AUTHORITY::WRITE)
					{
						return false;
					}
				}
			}
		}
		else if (type.compare("current") == 0 && path.size() > 1)
		{
			if (userGroup != inode.groupId)
			{
				if (inode.authority[7] != AUTHORITY::WRITE)
				{
					return false;
				}
			}
			else
			{
				if (userId != inode.ownerId)
				{
					if (inode.authority[4] != AUTHORITY::WRITE)
					{
						return false;
					}
				}
			}
		}

	}
	return true;
}

//回收盘块&inode
void Recycle(int addr)
{
	//回收盘块
	//将数据块盘号放入空闲盘块栈
	if (superblock.spareBlock[SPARESTACKNUM] == 49)//目前空闲盘块栈已满
	{
		fseek(file, DATASTART + BLOCKSIZE * addr, SEEK_SET);
		fwrite(&superblock.spareBlock, sizeof(int) * (SPARESTACKNUM + 1), 1, file);
		superblock.spareBlock[SPARESTACKNUM] = 0;
		superblock.spareBlock[superblock.spareBlock[SPARESTACKNUM]] = addr;
	}
	else
	{
		superblock.spareBlock[superblock.spareBlock[SPARESTACKNUM] + 1] = addr;
		superblock.spareBlock[SPARESTACKNUM] += 1;
	}
	superblock.spareBlockNum += 1;
	fseek(file, 0, SEEK_SET);
	fwrite(&superblock, sizeof(SuperBlock), 1, file);

}

//分割字符串
vector<string> SplitString(const string& s, const string& c)
{
	vector<string> v;
	string tem = s;
	while (true)
	{
		size_t n = tem.find(c);
		if (n == string::npos)
		{
			v.push_back(tem);
			break;
		}
		else
		{
			v.push_back(tem.substr(0, n));
			tem = tem.substr(n + 1);
		}
	}

	return v;
}

//解析路径
bool AnalyzePath(string para, direct & direct, inode & inode)
{
	//判断时绝对路径还是相对路径
	vector<string> v = SplitString(para, "/");
	direct = dir;
	inode = currentInode;
	if (para.find("/") != 0)
	{

		vector<string> p = path;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			char dirName[MAXNAMELENGTH];
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(direct.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * direct.inodeID[j], SEEK_SET);
					fread(&inode, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * inode.addr[0], SEEK_SET);
					fread(&direct, sizeof(direct), 1, file);
					flag = true;
					p.push_back(dirName);
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return false;
			}
		}
		dir = direct;
		currentInode = inode;
		path = p;
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			inode = rootInode;
			vector<string> p;
			p.push_back("home");
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&direct, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				char dirName[MAXNAMELENGTH];
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(direct.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * direct.inodeID[j], SEEK_SET);
						fread(&inode, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * inode.addr[0], SEEK_SET);
						fread(&direct, sizeof(direct), 1, file);
						flag = true;
						p.push_back(dirName);
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return false;
				}
			}
			path = p;
			currentInode = inode;
			dir = direct;
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return false;
		}
	}
	return true;
}

//检查读权限
bool CheckRead(inode inode)
{
	if (userGroup != 0)
	{
		if (userGroup != inode.groupId)
		{
			if (inode.authority[6] != AUTHORITY::READ)
			{
				return false;
			}
		}
		else
		{
			if (userId != inode.ownerId)
			{
				if (inode.authority[3] != AUTHORITY::WRITE)
				{
					return false;
				}
			}
		}
	}
	return true;
}

void Ls()
{

	fseek(file, DATASTART + BLOCKSIZE * currentInode.addr[0], SEEK_SET);
	fread(&dir, sizeof(direct), 1, file);
	fseek(file, INODESTART + INODESIZE * currentInode.inodeNo, SEEK_SET);
	fread(&currentInode, sizeof(inode), 1, file);
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strlen(dir.fileName[i]) == 0)
			continue;
		inode itemp;
		fseek(file, INODESTART + INODESIZE * dir.inodeID[i], SEEK_SET);
		fread(&itemp, sizeof(inode), 1, file);
		int count = 0;
		if (itemp.mode == 'd')
			count = itemp.dircount;
		else
			count = itemp.count;
		string au;
		for (int i = 0; i < 9; i++)
			au += itemp.authority[i];
		cout << itemp.mode << " " << au << " " << count << " " << accounts.userName[itemp.ownerId]
			<< " " << accounts.userGroup[itemp.groupId] << " " << itemp.fileSize << " " << itemp.lastTime
			<< " " << dir.fileName[i] << endl;
	}
}

void Pwd()
{
	string pa = "/home";
	for (int i = 1; i < path.size(); i++)
	{
		pa += "/" + path[i];
	}
	cout << pa << endl;
}

void Cd(string para)
{
	if (para.compare(".") == 0)
		return;
	if (para.compare("..") == 0)
	{
		fseek(file, INODESTART + INODESIZE * dir.inodeID[1], SEEK_SET);
		fread(&currentInode, sizeof(inode), 1, file);
		fseek(file, DATASTART + BLOCKSIZE * currentInode.addr[0], SEEK_SET);
		fread(&dir, sizeof(direct), 1, file);
		path.pop_back();
		return;
	}
	direct tem = dir;
	inode item = currentInode;
	direct originDir = dir;
	inode originInode = currentInode;
	vector<string> originPath = path;
	if (!AnalyzePath(para, tem, item))
		return;

	if (!CheckRead(item))
	{
		path = originPath;
		currentInode = originInode;
		dir = originDir;
		cout << "您没有读取该目录的权限" << endl;
		return;
	}
}

void Mkdir(string para)
{
	if (!CheckWrite(currentInode, "current"))
	{
		cout << "您没有权限在本目录下创建文件夹" << endl;
		return;
	}

	//检测能否创建文件
	if (para.length() == 0)
	{
		cout << "无效命令:mkdir需要明确参数" << endl;
		return;
	}
	if (para.length() > MAXNAMELENGTH)
	{
		cout << "文件名过长无法创建该文件" << endl;
		return;
	}

	if (superblock.spareInodeNum <= 0 || superblock.spareBlockNum <= 0)
	{
		cout << "空间已满无法创建文件" << endl;
		return;
	}

	char dirName[MAXNAMELENGTH];
	strcpy(dirName, para.c_str());
	//检查重名文件
	if (!CheckRepetedName(para, dir))
	{
		cout << "存在同名文件夹，无法创建该文件夹" << endl;
		return;
	}
	//检查当前目录是否已满
	if (currentInode.dircount + currentInode.count >= DIRMAXFILENUM)
	{
		cout << "当前目录已满，无法创建文件夹" << endl;
		return;
	}

	//创建新的inode
	inode subInode;
	subInode.inodeNo = UpdateInodeMap();
	subInode.fileSize = 0;
	subInode.dircount = 2;//默认两个子目录. ..
	subInode.mode = FILEMODE::DIR;
	memset(subInode.addr, -1, sizeof(int)*ADDRSIZE);
	//分配数据块
	if (superblock.spareBlock[SPARESTACKNUM] == 0)
	{
		cout << "当前盘已空" << endl;
		if (superblock.spareBlock[0] == 0)
		{
			cout << "磁盘已满,无法创建目录请清理磁盘后再进行操作" << endl;
			return;
		}
		else
		{
			//更新空闲盘块栈
			fseek(file, DATASTART + BLOCKSIZE * superblock.spareBlock[0], SEEK_SET);
			fread(&superblock.spareBlock, sizeof(int)*(SPARESTACKNUM + 1), 1, file);
			subInode.addr[0] = superblock.spareBlock[0];
		}
	}
	else
	{
		subInode.addr[0] = SPARESTACKNUM - 1 - superblock.spareBlock[SPARESTACKNUM];
		superblock.spareBlock[SPARESTACKNUM] -= 1;
	}

	strcpy(subInode.lastTime, getTime());
	subInode.ownerId = userId;
	subInode.groupId = userGroup;
	setAuthority('d', subInode.authority);
	//将indoe写入索引表区
	fseek(file, INODESTART + INODESIZE * subInode.inodeNo, SEEK_SET);
	fwrite(&subInode, sizeof(inode), 1, file);

	//创建目录文件
	direct subDir;
	memset(subDir.fileName, 0, sizeof(char) * 20 * MAXNAMELENGTH);
	memset(subDir.inodeID, -1, sizeof(int)*DIRMAXFILENUM);
	strcpy(subDir.fileName[0], ".");
	subDir.inodeID[0] = subInode.inodeNo;
	strcpy(subDir.fileName[1], "..");
	subDir.inodeID[1] = currentInode.inodeNo;
	fseek(file, DATASTART + BLOCKSIZE * subInode.addr[0], SEEK_SET);
	fwrite(&subDir, sizeof(direct), 1, file);

	//更新当前inode
	currentInode.dircount += 1;
	strcpy(currentInode.lastTime, getTime());
	fseek(file, INODESTART + INODESIZE * currentInode.inodeNo, SEEK_SET);
	fwrite(&currentInode, sizeof(inode), 1, file);

	//更新目录文件
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strlen(dir.fileName[i]) == 0)
		{
			strcpy(dir.fileName[i], dirName);
			dir.inodeID[i] = subInode.inodeNo;
			break;
		}
	}
	fseek(file, DATASTART + currentInode.addr[0] * BLOCKSIZE, SEEK_SET);
	fwrite(&dir, sizeof(direct), 1, file);

	//更新超级块
	superblock.spareInodeNum -= 1;
	superblock.spareBlockNum -= 1;
	fseek(file, 0, SEEK_SET);
	fwrite(&superblock, sizeof(SuperBlock), 1, file);

}

void Rmdir(string filena)
{
	vector<string> v = SplitString(filena, "/");
	direct tem, originDir;
	inode item, originInode, preItem;
	char dirName[MAXNAMELENGTH];
	strcpy(dirName, v[v.size() - 1].c_str());
	int index;
	if (filena.find("/") != 0)
	{
		tem = dir;
		item = currentInode;
		originDir = tem;
		originInode = item;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			preItem = item;
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(tem.fileName[j], dirName) == 0)
				{
					index = j;
					fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
					fread(&item, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
					fread(&tem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			item = rootInode;
			originInode = item;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&tem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size() - 1; i++)
			{
				bool flag = false;
				preItem = item;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(tem.fileName[j], dirName) == 0)
					{
						index = j;
						fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
						fread(&item, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
						fread(&tem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}

	if (!CheckWrite(item, "sub"))
	{
		cout << "您没有权限删除该文件夹" << endl;
		return;
	}
	//判断该目录是否为空
	bool isEmpty = true;
	for (int i = 2; i < DIRMAXFILENUM; i++)
	{
		if (strlen(tem.fileName[i]) > 0)
			isEmpty = false;
	}
	if (!isEmpty)
	{
		cout << "该目录非空，请删除其内容后再进行操作" << endl;
		return;
	}

	//回收盘块
	Recycle(item.addr[0]);

	//回收inode
	inodeMap[item.inodeNo] = 0;
	fseek(file, INODEMAPSTART, SEEK_SET);
	fwrite(&inodeMap, INODEMAPSIZE, 1, file);

	//清除父目录的数据
	direct preDir;
	fseek(file, DATASTART + BLOCKSIZE * preItem.addr[0], SEEK_SET);
	fread(&preDir, sizeof(direct), 1, file);
	for (int i = 0; i < MAXNAMELENGTH; i++)
		preDir.fileName[index][i] = NULL;
	preDir.inodeID[index] = -1;
	fseek(file, DATASTART + BLOCKSIZE * preItem.addr[0], SEEK_SET);
	fwrite(&preDir, sizeof(direct), 1, file);
}

void Passwd()
{
	inode userInode;
	fseek(file, INODESTART + INODESIZE, SEEK_SET);
	fread(&userInode, sizeof(inode), 1, file);
	string password;
	while (true)
	{
		cout << "请输入密码:";
		password = Encryption();
		char pas[MAXNAMELENGTH];
		strcpy(pas, password.c_str());
		if (strcmp(pas, accounts.userPsw[userId]) == 0)
		{
			string newPsw, reNewPsw;
			cout << "请输入新密码:";
			newPsw = Encryption();
			if (newPsw.length() > MAXNAMELENGTH)
			{
				cout << "密码过长，请重新输入" << endl;
				continue;
			}
			cout << "请确认新密码:";
			reNewPsw = Encryption();
			if (newPsw.compare(reNewPsw) == 0)
			{
				cout << "用户口令修改成功" << endl;
				strcpy(pas, newPsw.c_str());
				strcpy(accounts.userPsw[userId], pas);
				//占据第二个数据块
				fseek(file, DATASTART + BLOCKSIZE, SEEK_SET);
				fwrite(&accounts, sizeof(user), 1, file);
				break;
			}
			else
			{
				cout << "两次密码输入不一致，请重新输入" << endl;
				continue;
			}
		}
		else
		{
			cout << "密码错误，请重新输入" << endl;
			continue;
		}
	}
}


void Umask(string para)
{
	if (para.length() == 0)
	{
		for (int i = 0; i < 3; i++)
			cout << UMASK[i];
		cout << endl;
		return;
	}
	for (int i = 0; i < para.length(); i++)
	{
		UMASK[i] = (int)(para[i] - '0');
	}
}

vector<char> InputContent(string size)
{
	vector<string> t;
	t = SplitString(size, "*");
	long filesize = 1;
	for (int i = 0; i < t.size(); i++)
	{
		filesize *= atoi(t[i].c_str());
	}
	vector<char> content;
	for (int i = 0; i < filesize; i++)
	{
		if (i % BLOCKSIZE == 0)
			content.push_back('W');
		else
			content.push_back('a');

	}
	return content;
}

void Vi(string fileName, vector<char> content)
{
	vector<string> v = SplitString(fileName, "/");
	direct tem;
	inode item;
	char dirName[MAXNAMELENGTH];
	int index;
	if (fileName.find("/") != 0)
	{
		tem = dir;
		item = currentInode;
		for (int i = 0; i < v.size() - 1; i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(tem.fileName[j], dirName) == 0)
				{
					index = j;
					fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
					fread(&item, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
					fread(&tem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			item = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&tem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size() - 1; i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(tem.fileName[j], dirName) == 0)
					{
						index = j;
						fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
						fread(&item, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
						fread(&tem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}
	//检查权限
	if (!CheckWrite(currentInode, "current"))
	{
		cout << "您没有权限在本目录下创建文件" << endl;
		return;
	}

	//检测能否创建文件
	if (fileName.length() == 0)
	{
		cout << "无效命令:vi需要明确参数" << endl;
		return;
	}
	if (fileName.length() > MAXNAMELENGTH)
	{
		cout << "文件名过长无法创建该文件" << endl;
		return;
	}

	if (superblock.spareInodeNum <= 0)
	{
		cout << "空间已满无法创建文件" << endl;
		return;
	}
	//检验是否有重名文件
	strcpy(dirName, v[v.size() - 1].c_str());
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strcmp(dirName, tem.fileName[i]) == 0)
		{
			cout << "存在同名文件，无法创建该文件" << endl;
			return;
		}
	}
	//检查当前目录是否已满
	if (currentInode.dircount >= DIRMAXFILENUM)
	{
		cout << "当前目录已满，无法创建文件" << endl;
		return;
	}

	long FILESIZE = content.size();
	int blocks = FILESIZE / BLOCKSIZE;//计算该文件所需要的盘块数
	if (FILESIZE%BLOCKSIZE != 0 || FILESIZE == 0)
		blocks++;
	//判断当前剩余空间能否满足该文件
	if (blocks > superblock.spareBlockNum)
	{
		cout << "当前磁盘空间不足，无法创建该文件" << endl;
		return;
	}

	//创建新的inode
	inode fileInode;
	for (int i = 0; i < INODETOTALNUM; i++)
	{
		if (inodeMap[i] != 1)
		{
			fileInode.inodeNo = i;
			inodeMap[i] = 1;//更新索引节点表
			break;
		}
	}
	//将索引节点表写入
	fseek(file, INODEMAPSTART, SEEK_SET);
	fwrite(&inodeMap, INODEMAPSIZE, 1, file);

	fileInode.fileSize = FILESIZE;
	fileInode.count = 1;//默认一个inode链接
	fileInode.mode = FILEMODE::FIL;
	memset(fileInode.addr, -1, sizeof(int)*ADDRSIZE);
	int firstAddressing[ADDRBLOCKNUM];
	int secondAddr_1[ADDRBLOCKNUM];
	int secondAddressing[ADDRBLOCKNUM];
	for (int i = 0; i < ADDRBLOCKNUM; i++)
	{
		firstAddressing[i] = -1;
		secondAddressing[i] = -1;
	}
	bool flag = true;
	int saddr1Index = 0;
	int saddrIndex = 0;
	bool isLast = false;
	//分配数据块
	for (int i = 0; i < blocks; i++)
	{
		int first = -1;//当前空闲盘块栈指向栈底时存储该盘块号
		if (superblock.spareBlock[SPARESTACKNUM] == 0)
		{
			if (superblock.spareBlock[0] == 0)
			{
				cout << "磁盘已满,无法创建文件请清理磁盘后再进行操作" << endl;
				return;
			}
			else
			{
				//更新空闲盘块栈
				fseek(file, DATASTART + BLOCKSIZE * superblock.spareBlock[0], SEEK_SET);
				fread(&superblock.spareBlock, sizeof(int)*(SPARESTACKNUM + 1), 1, file);
				first = superblock.spareBlock[0];
			}
		}

		if (i < ADDRSIZE - 1 && flag)
		{
			superblock.spareBlockNum -= 1;
			if (first >= 0)
			{
				fileInode.addr[i] = first;
			}
			else
			{
				fileInode.addr[i] = SPARESTACKNUM - 1 - superblock.spareBlock[SPARESTACKNUM];
				superblock.spareBlock[SPARESTACKNUM] -= 1;
			}
			if (i == ADDRSIZE - 2)
			{
				flag = false;
				i--;
				continue;
			}
			char tempContent[BLOCKSIZE];
			for (int j = 0; j < BLOCKSIZE; j++)
			{
				if (i*BLOCKSIZE + j >= content.size())
					tempContent[j] = NULL;
				else
					tempContent[j] = content[i*BLOCKSIZE + j];
			}
			//将文件内容写入数据区
			fseek(file, DATASTART + BLOCKSIZE * fileInode.addr[i], SEEK_SET);
			fwrite(&tempContent, sizeof(char), BLOCKSIZE, file);

		}
		else if (i < ADDRSIZE - 2 + ADDRBLOCKNUM && !flag)
		{
			superblock.spareBlockNum -= 1;
			if (first >= 0)
			{
				firstAddressing[i - (ADDRSIZE - 2)] = first;
			}
			else
			{
				firstAddressing[i - (ADDRSIZE - 2)] = SPARESTACKNUM - 1 - superblock.spareBlock[SPARESTACKNUM];
				superblock.spareBlock[SPARESTACKNUM] -= 1;
			}
			char tempContent[BLOCKSIZE];
			for (int j = 0; j < BLOCKSIZE; j++)
			{
				if (i*BLOCKSIZE + j >= content.size())
					tempContent[j] = NULL;
				else
					tempContent[j] = content[i*BLOCKSIZE + j];
			}
			//将文件内容写入数据区
			fseek(file, DATASTART + BLOCKSIZE * firstAddressing[i - (ADDRSIZE - 2)], SEEK_SET);
			fwrite(&tempContent, sizeof(char), BLOCKSIZE, file);
		}
		else if (i == ADDRSIZE - 2 + ADDRBLOCKNUM && !flag)
		{
			superblock.spareBlockNum -= 1;
			if (first >= 0)
			{
				fileInode.addr[ADDRSIZE - 1] = first;
			}
			else
			{
				fileInode.addr[ADDRSIZE - 1] = SPARESTACKNUM - 1 - superblock.spareBlock[SPARESTACKNUM];
				superblock.spareBlock[SPARESTACKNUM] -= 1;
			}
			i--;
			flag = true;

		}
		else
		{
			if (saddr1Index < ADDRBLOCKNUM)
			{
				superblock.spareBlockNum -= 1;
				if (first > 0)
				{
					secondAddr_1[saddr1Index] = first;
				}
				else
				{
					secondAddr_1[saddr1Index] = SPARESTACKNUM - 1 - superblock.spareBlock[SPARESTACKNUM];
					superblock.spareBlock[SPARESTACKNUM] -= 1;
				}
				char tempContent[BLOCKSIZE];
				for (int j = 0; j < BLOCKSIZE; j++)
				{
					if (i*BLOCKSIZE + j >= content.size())
						tempContent[j] = NULL;
					else
						tempContent[j] = content[i*BLOCKSIZE + j];
				}
				//将文件内容写入数据区
				fseek(file, DATASTART + BLOCKSIZE * secondAddr_1[saddr1Index], SEEK_SET);
				fwrite(&tempContent, sizeof(char), BLOCKSIZE, file);
				saddr1Index++;

			}
			else if (saddr1Index >= ADDRBLOCKNUM)
			{
				superblock.spareBlockNum -= 1;
				if (first > 0)
				{
					secondAddressing[saddrIndex] = first;
				}
				else
				{
					secondAddressing[saddrIndex] = SPARESTACKNUM - 1 - superblock.spareBlock[SPARESTACKNUM];
					superblock.spareBlock[SPARESTACKNUM] -= 1;
				}
				fseek(file, DATASTART + BLOCKSIZE * secondAddressing[saddrIndex], SEEK_SET);
				fwrite(&secondAddr_1, sizeof(int), ADDRBLOCKNUM, file);
				for (int i = 0; i < ADDRBLOCKNUM; i++)
					secondAddr_1[i] = NULL;
				saddr1Index = 0;
				saddrIndex++;
				i--;
			}
			if (i == blocks - 1)
			{
				first = -1;
				if (superblock.spareBlock[SPARESTACKNUM] == 0)
				{
					if (superblock.spareBlock[0] == 0)
					{
						cout << "磁盘已满,无法创建文件请清理磁盘后再进行操作" << endl;
						return;
					}
					else
					{
						fseek(file, DATASTART + BLOCKSIZE * superblock.spareBlock[0], SEEK_SET);
						fread(&superblock.spareBlock, sizeof(int)*(SPARESTACKNUM + 1), 1, file);
						first = superblock.spareBlock[0];
					}
				}
				superblock.spareBlockNum -= 1;
				if (first > 0)
				{
					secondAddressing[saddrIndex] = first;
				}
				else
				{
					secondAddressing[saddrIndex] = SPARESTACKNUM - 1 - superblock.spareBlock[SPARESTACKNUM];
					superblock.spareBlock[SPARESTACKNUM] -= 1;
				}
				fseek(file, DATASTART + BLOCKSIZE * secondAddressing[saddrIndex], SEEK_SET);
				fwrite(&secondAddr_1, sizeof(int), ADDRBLOCKNUM, file);
			}

		}
	}
	//将地址间接寻址写入数据块中
	if (firstAddressing[0] >= 0)
	{
		fseek(file, DATASTART + BLOCKSIZE * fileInode.addr[ADDRSIZE - 2], SEEK_SET);
		fwrite(&firstAddressing, sizeof(int), ADDRBLOCKNUM, file);
	}
	if (secondAddressing[0] >= 0)
	{
		fseek(file, DATASTART + BLOCKSIZE * fileInode.addr[ADDRSIZE - 1], SEEK_SET);
		fwrite(&secondAddressing, sizeof(int), ADDRBLOCKNUM, file);
	}
	fseek(file, 0, SEEK_SET);
	fwrite(&superblock, sizeof(SuperBlock), 1, file);

	strcpy(fileInode.lastTime, getTime());
	fileInode.ownerId = userId;
	fileInode.groupId = userGroup;
	setAuthority('-', fileInode.authority);
	//将indoe写入索引表区
	fseek(file, INODESTART + INODESIZE * fileInode.inodeNo, SEEK_SET);
	fwrite(&fileInode, sizeof(inode), 1, file);

	//更新当前inode
	item.count += 1;
	strcpy(item.lastTime, getTime());
	fseek(file, INODESTART + INODESIZE * item.inodeNo, SEEK_SET);
	fwrite(&item, sizeof(inode), 1, file);

	//更新目录文件
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strlen(tem.fileName[i]) == 0)
		{
			strcpy(tem.fileName[i], dirName);
			tem.inodeID[i] = fileInode.inodeNo;
			break;
		}
	}
	fseek(file, DATASTART + item.addr[0] * BLOCKSIZE, SEEK_SET);
	fwrite(&tem, sizeof(direct), 1, file);

	//更新超级块
	superblock.spareInodeNum -= 1;
	fseek(file, 0, SEEK_SET);
	fwrite(&superblock, sizeof(SuperBlock), 1, file);

}

void Mv(string sfile, string dfile)
{
	//检查权限
	if (!CheckWrite(currentInode, "current"))
	{
		cout << "您没有权限在本目录下进行操作" << endl;
		return;
	}
	char sname[MAXNAMELENGTH], dname[MAXNAMELENGTH];
	strcpy(sname, sfile.c_str());
	strcpy(dname, dfile.c_str());

	//判断时绝对路径还是相对路径
	vector<string> v = SplitString(sfile, "/");
	direct tem;
	inode item;
	char dirName[MAXNAMELENGTH];
	if (sfile.find("/") != 0)
	{
		tem = dir;
		item = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(tem.fileName[j], dirName) == 0)
				{
					if (i == v.size() - 1)
					{
						flag = true;
						break;
					}
					fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
					fread(&item, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
					fread(&tem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			item = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&tem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(tem.fileName[j], dirName) == 0)
					{
						if (i == v.size() - 1)
						{
							flag = true;
							break;
						}
						fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
						fread(&item, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
						fread(&tem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}

	//判断是否重名
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strcmp(dname, tem.fileName[i]) == 0)
		{
			cout << "目标文件名在当前目录下重复，无法重新命名" << endl;
			return;
		}
	}

	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strcmp(dirName, tem.fileName[i]) == 0)
		{
			strcpy(tem.fileName[i], dname);
			fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
			fwrite(&tem, sizeof(direct), 1, file);
			return;
		}
	}
	cout << "系统找不到指定文件" << endl;
}
void Chmod(string type, string filena)
{
	char filename[MAXNAMELENGTH];
	strcpy(filename, filena.c_str());
	//判断权限
	vector<string> v = SplitString(filena, "/");
	direct tem;
	inode item;
	char dirName[MAXNAMELENGTH];
	if (filena.find("/") != 0)
	{
		tem = dir;
		item = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(tem.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
					fread(&item, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
					fread(&tem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			item = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&tem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(tem.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
						fread(&item, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
						fread(&tem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}
	if (userGroup != 0)
	{
		cout << "您无权修改该文件使用权限" << endl;
		return;
	}

	//解析命令
	vector<char> user;
	vector<char> order;
	char operation = '1';
	bool flag = true;
	bool wrong = false;
	for (int i = 0; i < type.length(); i++)
	{
		if (type[i] == '+' || type[i] == '-' || type[i] == '=')
		{
			if (user.size() == 0)
			{
				wrong = true;
				break;
			}
			operation = type[i];
			flag = false;
		}
		else
		{
			if (flag)
			{
				if (type[i] == 'u' || type[i] == 'g' || type[i] == 'o')
					user.push_back(type[i]);
				else
				{
					wrong = true;
					break;
				}
			}
			else
			{
				if (type[i] == 'r' || type[i] == 'w' || type[i] == 'x')
					order.push_back(type[i]);
				else
				{
					wrong = true;
					break;
				}
			}

		}
	}
	if (wrong)
	{
		cout << "无效命令:chmod 类型错误" << endl;
		return;
	}
	//修改权限
	for (int i = 0; i < user.size(); i++)
	{
		int index = -1;
		if (user[i] == 'u')
			index = 0;
		else if (user[i] == 'g')
			index = 3;
		else if (user[i] == 'o')
			index = 6;
		if (index >= 0)
		{
			if (operation == '+')
			{
				for (int j = 0; j < order.size(); j++)
				{
					if (order[j] == 'r')
						item.authority[index + 0] = 'r';
					else if (order[j] == 'w')
						item.authority[index + 1] = 'w';
					else
						item.authority[index + 2] = 'x';
				}
			}
			else if (operation == '-')
			{
				for (int j = 0; j < order.size(); j++)
				{
					if (order[j] == 'r')
						item.authority[index + 0] = '-';
					else if (order[j] == 'w')
						item.authority[index + 1] = '-';
					else
						item.authority[index + 2] = '-';
				}
			}
			else if (operation == '=')
			{
				for (int j = 0; j < order.size(); j++)
				{
					if (order[j] == 'r')
						item.authority[index + 0] = 'r';
					else if (order[j] == 'w')
						item.authority[index + 1] = 'w';
					else
						item.authority[index + 2] = 'x';
				}
			}
		}

	}
	fseek(file, INODESTART + INODESIZE * item.inodeNo, SEEK_SET);
	fwrite(&item, sizeof(inode), 1, file);
}

void Chown(string user, string filena)
{
	char filename[MAXNAMELENGTH], username[MAXNAMELENGTH];
	strcpy(filename, filena.c_str());
	strcpy(username, user.c_str());
	//判断权限
	vector<string> v = SplitString(filena, "/");
	direct tem;
	inode item;
	char dirName[MAXNAMELENGTH];
	if (filena.find("/") != 0)
	{
		tem = dir;
		item = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(tem.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
					fread(&item, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
					fread(&tem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			item = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&tem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(tem.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
						fread(&item, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
						fread(&tem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}
	if (userGroup != 0)
	{
		cout << "您无权修改该文件拥有者" << endl;
		return;
	}

	int id;
	bool flag = false;
	for (int i = 0; i < MAXUSERNUM; i++)
	{
		if (strcmp(username, accounts.userName[i]) == 0)
		{
			id = accounts.userId[i];
			if (accounts.userGroup[i] != item.groupId)
			{
				cout << "该用户是其他组成员，无法修改拥有者" << endl;
				return;
			}
			flag = true;
			break;
		}
	}
	if (!flag)
	{
		cout << "该用户不存在" << endl;
		return;
	}
	item.ownerId = id;
	fseek(file, INODESTART + INODESIZE * item.inodeNo, SEEK_SET);
	fwrite(&item, sizeof(inode), 1, file);
}

void Chgrp(string group, string filena)
{
	char filename[MAXNAMELENGTH];
	strcpy(filename, filena.c_str());
	//判断权限
	vector<string> v = SplitString(filena, "/");
	direct tem;
	inode item;
	char dirName[MAXNAMELENGTH];
	if (filena.find("/") != 0)
	{
		tem = dir;
		item = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(tem.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
					fread(&item, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
					fread(&tem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			item = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&tem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(tem.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
						fread(&item, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
						fread(&tem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}
	if (userGroup != 0)
	{
		cout << "您无权修改该文件群组" << endl;
		return;
	}
	int n = atoi(group.c_str());
	bool flag = false;
	for (int i = 0; i < MAXUSERNUM; i++)
	{
		if (n == accounts.userGroup[i])
		{
			flag = true;
			break;
		}
	}
	if (!flag)
	{
		cout << "该群组不存在，请重新输入" << endl;
		return;
	}
	item.groupId = n;
	fseek(file, INODESTART + INODESIZE * item.inodeNo, SEEK_SET);
	fwrite(&item, sizeof(inode), 1, file);
}

void Cat(string filena)
{
	//判断权限
	vector<string> v = SplitString(filena, "/");
	direct tem;
	inode item;
	char dirName[MAXNAMELENGTH];
	if (filena.find("/") != 0)
	{
		tem = dir;
		item = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(tem.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
					fread(&item, sizeof(inode), 1, file);
					if (i == v.size() - 1)
					{
						if (item.mode == FILEMODE::DIR)
						{
							cout << "这是一个目录" << endl;
							return;
						}
					}
					else
					{
						fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
						fread(&tem, sizeof(direct), 1, file);
					}
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			item = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&tem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(tem.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
						fread(&item, sizeof(inode), 1, file);
						if (i == v.size() - 1)
						{
							if (item.mode == FILEMODE::DIR)
							{
								cout << "这是一个目录" << endl;
								return;
							}
						}
						else
						{
							fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
							fread(&tem, sizeof(direct), 1, file);
						}
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}
	if (!CheckRead(item))
	{
		cout << "您无权查看该文件" << endl;
		return;
	}

	int blocks = item.fileSize / BLOCKSIZE;
	if (item.fileSize % BLOCKSIZE != 0)
		blocks++;
	int firstAddressing[ADDRBLOCKNUM];
	int secondAddressing[ADDRBLOCKNUM];
	int secondAddr_1[ADDRBLOCKNUM];
	bool flag = true;
	for (int i = 0; i < blocks; i++)
	{
		if (i < ADDRSIZE - 2)
		{
			char content[BLOCKSIZE];
			fseek(file, DATASTART + BLOCKSIZE * item.addr[i], SEEK_SET);
			fread(&content, sizeof(char), BLOCKSIZE, file);
			for (int i = 0; i < BLOCKSIZE; i++)
				cout << content[i];
			if (i == blocks - 1)
				cout << endl;
		}
		else if (i == ADDRSIZE - 2 && flag)
		{
			fseek(file, DATASTART + BLOCKSIZE * item.addr[i], SEEK_SET);
			fread(&firstAddressing, sizeof(int)*ADDRBLOCKNUM, 1, file);
			i--;
			flag = false;;
		}
		else if (i < ADDRSIZE - 2 + ADDRBLOCKNUM && !flag)
		{
			char content[BLOCKSIZE];
			fseek(file, DATASTART + BLOCKSIZE * firstAddressing[i - (ADDRSIZE - 2)], SEEK_SET);
			fread(&content, sizeof(char), BLOCKSIZE, file);
			for (int i = 0; i < BLOCKSIZE; i++)
				cout << content[i];
			if (i == blocks - 1)
				cout << endl;
		}
		else if (i == ADDRSIZE - 1 + ADDRBLOCKNUM && !flag)
		{
			fseek(file, DATASTART + BLOCKSIZE * item.addr[i], SEEK_SET);
			fread(&secondAddressing, sizeof(int)*ADDRBLOCKNUM, 1, file);
			i--;
			flag = true;
		}
		else
		{
			for (int j = 0; j < ADDRBLOCKNUM; j++)
			{
				if (secondAddressing[j] < 0 || i + j >= blocks)
					break;
				fseek(file, DATASTART + BLOCKSIZE * secondAddressing[j], SEEK_SET);
				fread(&secondAddr_1, sizeof(int)*ADDRBLOCKNUM, 1, file);
				for (int k = 0; k < ADDRBLOCKNUM; k++)
				{
					if (secondAddr_1[k] == -1)
						break;
					char content[BLOCKSIZE];
					fseek(file, DATASTART + BLOCKSIZE * secondAddr_1[k], SEEK_SET);
					fread(&content, sizeof(char), BLOCKSIZE, file);
					for (int i = 0; i < BLOCKSIZE; i++)
						cout << content[i];
					if (i == blocks - 1)
						cout << endl;
				}
			}
			break;
		}
	}
	cout << endl;

}


void Cp(string sname, string dname)
{
	//判断权限
	vector<string> v = SplitString(sname, "/");
	direct stem;
	inode sitem;
	char dirName[MAXNAMELENGTH];
	string pathName = dname + "/";
	if (sname.find("/") != 0)
	{
		stem = dir;
		sitem = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(stem.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * stem.inodeID[j], SEEK_SET);
					fread(&sitem, sizeof(inode), 1, file);
					if (i == v.size() - 1)
					{
						if (sitem.mode == FILEMODE::DIR)
						{
							cout << "无效命令:cp [原文件] [目标地址]" << endl;
							return;
						}
					}
					else
					{
						fseek(file, DATASTART + BLOCKSIZE * sitem.addr[0], SEEK_SET);
						fread(&stem, sizeof(direct), 1, file);
					}
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			sitem = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&stem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(stem.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * stem.inodeID[j], SEEK_SET);
						fread(&sitem, sizeof(inode), 1, file);
						if (i == v.size() - 1)
						{
							if (sitem.mode == FILEMODE::DIR)
							{
								cout << "无效命令:cp [原文件] [目标地址]" << endl;
								return;
							}
						}
						else
						{
							fseek(file, DATASTART + BLOCKSIZE * sitem.addr[0], SEEK_SET);
							fread(&stem, sizeof(direct), 1, file);
						}
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}
	string filename = v[v.size() - 1];
	pathName += filename;

	v = SplitString(dname, "/");
	direct dtem;
	inode ditem;
	if (dname.find("./") == 0)
	{
		dtem = dir;
		ditem = currentInode;
		pathName = filename;
	}
	else if (dname.find("/") != 0)
	{
		dtem = dir;
		ditem = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(dtem.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * dtem.inodeID[j], SEEK_SET);
					fread(&ditem, sizeof(inode), 1, file);
					if (i == v.size() - 1)
					{
						if (ditem.mode == FILEMODE::FIL)
						{
							cout << "无效命令:cp [原文件] [目标地址]" << endl;
							return;
						}
					}
					fseek(file, DATASTART + BLOCKSIZE * ditem.addr[0], SEEK_SET);
					fread(&dtem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			ditem = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&dtem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(dtem.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * dtem.inodeID[j], SEEK_SET);
						fread(&ditem, sizeof(inode), 1, file);
						if (i == v.size() - 1)
						{
							if (ditem.mode == FILEMODE::FIL)
							{
								cout << "无效命令:cp [原文件] [目标地址]" << endl;
								return;
							}
						}
						fseek(file, DATASTART + BLOCKSIZE * ditem.addr[0], SEEK_SET);
						fread(&dtem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}

	if (!CheckWrite(sitem, "sub"))
	{
		cout << "您没有对该文件进行复制的权限" << endl;
		return;
	}
	if (!CheckWrite(ditem, "sub"))
	{
		cout << "您没有对该文件夹进行操作的权限" << endl;
		return;
	}

	//检测能否创建文件
	if (superblock.spareInodeNum <= 0 || superblock.spareBlockNum <= 0)
	{
		cout << "空间已满无法创建文件" << endl;
		return;
	}
	//检验是否有重名文件
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strcmp(dirName, dtem.fileName[i]) == 0)
		{
			cout << "存在同名文件，无法复制该文件" << endl;
			return;
		}
	}
	//检查当前目录是否已满
	if (currentInode.dircount >= DIRMAXFILENUM)
	{
		cout << "当前目录已满，无法复制该文件" << endl;
		return;
	}

	int blocks = sitem.fileSize / BLOCKSIZE;
	if (sitem.fileSize % BLOCKSIZE != 0)
		blocks++;
	int firstAddressing[ADDRBLOCKNUM];
	int secondAddressing[ADDRBLOCKNUM];
	int secondAddr_1[ADDRBLOCKNUM];
	bool flag = true;
	vector<char> conten;
	for (int i = 0; i < blocks; i++)
	{
		if (i < ADDRSIZE - 2)
		{
			char content[BLOCKSIZE];
			fseek(file, DATASTART + BLOCKSIZE * sitem.addr[i], SEEK_SET);
			fread(&content, sizeof(char), BLOCKSIZE, file);
			for (int i = 0; i < BLOCKSIZE; i++)
				conten.push_back(content[i]);
		}
		else if (i == ADDRSIZE - 2 && flag)
		{
			fseek(file, DATASTART + BLOCKSIZE * sitem.addr[i], SEEK_SET);
			fread(&firstAddressing, sizeof(int)*ADDRBLOCKNUM, 1, file);
			i--;
			flag = false;;
		}
		else if (i < ADDRSIZE - 2 + ADDRBLOCKNUM && !flag)
		{
			char content[BLOCKSIZE];
			fseek(file, DATASTART + BLOCKSIZE * firstAddressing[i - (ADDRSIZE - 2)], SEEK_SET);
			fread(&content, sizeof(char), BLOCKSIZE, file);
			for (int i = 0; i < BLOCKSIZE; i++)
				conten.push_back(content[i]);
		}
		else if (i == ADDRSIZE - 1 + ADDRBLOCKNUM && !flag)
		{
			fseek(file, DATASTART + BLOCKSIZE * sitem.addr[i], SEEK_SET);
			fread(&secondAddressing, sizeof(int)*ADDRBLOCKNUM, 1, file);
			i--;
			flag = true;
		}
		else
		{
			for (int j = 0; j < ADDRBLOCKNUM; j++)
			{
				if (secondAddressing[j] == -1)
					break;
				fseek(file, DATASTART + BLOCKSIZE * secondAddressing[j], SEEK_SET);
				fread(&secondAddr_1, sizeof(int)*ADDRBLOCKNUM, 1, file);
				for (int k = 0; k < ADDRBLOCKNUM; k++)
				{
					if (secondAddr_1[k] == -1)
						break;
					char content[BLOCKSIZE];
					fseek(file, DATASTART + BLOCKSIZE * secondAddr_1[k], SEEK_SET);
					fread(&content, sizeof(char), BLOCKSIZE, file);
					for (int i = 0; i < BLOCKSIZE; i++)
						conten.push_back(content[i]);
				}
			}
			break;
		}
	}
	Vi(pathName, conten);

}

void Rm(string filena)
{
	//判断权限
	vector<string> v = SplitString(filena, "/");
	direct tem;
	inode item, preItem;
	char dirName[MAXNAMELENGTH];
	int index;
	if (filena.find("/") != 0)
	{
		tem = dir;
		item = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			preItem = item;
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(tem.fileName[j], dirName) == 0)
				{
					index = j;
					fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
					fread(&item, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
					fread(&tem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			item = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&tem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				preItem = item;
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(tem.fileName[j], dirName) == 0)
					{
						index = j;
						fseek(file, INODESTART + INODESIZE * tem.inodeID[j], SEEK_SET);
						fread(&item, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * item.addr[0], SEEK_SET);
						fread(&tem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}

	if (!CheckWrite(item, "sub"))
	{
		cout << "您没有对该文件进行操作的权限" << endl;
		return;
	}
	//如果是目录判断该目录是否为空
	if (item.mode == FILEMODE::DIR)
	{
		cout << "这是一个目录" << endl;
		bool isEmpty = true;
		for (int i = 2; i < DIRMAXFILENUM; i++)
		{
			if (strlen(tem.fileName[i]) > 0)
				isEmpty = false;
		}
		if (!isEmpty)
		{
			cout << "该目录非空，请删除其内容后再进行操作" << endl;
			return;
		}
	}
	//回收inode
	inodeMap[item.inodeNo] = 0;
	fseek(file, INODEMAPSTART, SEEK_SET);
	fwrite(&inodeMap, INODEMAPSIZE, 1, file);

	//清除父目录的数据
	direct preDir;
	fseek(file, DATASTART + BLOCKSIZE * preItem.addr[0], SEEK_SET);
	fread(&preDir, sizeof(direct), 1, file);
	memset(preDir.fileName[index], 0, sizeof(char)*MAXNAMELENGTH);
	preDir.inodeID[index] = -1;
	fseek(file, DATASTART + BLOCKSIZE * preItem.addr[0], SEEK_SET);
	fwrite(&preDir, sizeof(direct), 1, file);

	//若是link文件则不回收盘块
	if (item.mode == FILEMODE::LINK)
		return;

	//回收盘块
	int blocks = item.fileSize / BLOCKSIZE;
	if (item.fileSize % BLOCKSIZE != 0 || blocks == 0)
		blocks++;
	int firstAddressing[ADDRBLOCKNUM];
	int secondAddressing[ADDRBLOCKNUM];
	int secondAddr_1[ADDRBLOCKNUM];
	bool flag = true;
	for (int i = 0; i < blocks; i++)
	{
		if (i < ADDRSIZE - 2)
			Recycle(item.addr[i]);
		else if (i == ADDRSIZE - 2 && flag)
		{
			fseek(file, DATASTART + BLOCKSIZE * item.addr[i], SEEK_SET);
			fread(&firstAddressing, sizeof(int)*ADDRBLOCKNUM, 1, file);
			Recycle(item.addr[i]);
			i--;
			flag = false;;
		}
		else if (i < ADDRSIZE - 2 + ADDRBLOCKNUM && !flag)
			Recycle(firstAddressing[i - (ADDRSIZE - 2)]);
		else if (i == ADDRSIZE - 1 + ADDRBLOCKNUM && !flag)
		{
			fseek(file, DATASTART + BLOCKSIZE * item.addr[i], SEEK_SET);
			fread(&secondAddressing, sizeof(int)*ADDRBLOCKNUM, 1, file);
			Recycle(item.addr[i]);
			i--;
			flag = true;
		}
		else
		{
			for (int j = 0; j < ADDRBLOCKNUM; j++)
			{
				if (secondAddressing[j] == -1)
					break;
				fseek(file, DATASTART + BLOCKSIZE * secondAddressing[j], SEEK_SET);
				fread(&secondAddr_1, sizeof(int)*ADDRBLOCKNUM, 1, file);
				Recycle(secondAddressing[j]);
				for (int k = 0; k < ADDRBLOCKNUM; k++)
				{
					if (secondAddr_1[k] == -1)
						break;
					Recycle(secondAddr_1[k]);
				}
			}
			break;
		}
	}
}

void Ln(string sname, string dname)
{
	//判断权限
	vector<string> v = SplitString(sname, "/");
	direct stem;
	inode sitem;
	char dirName[MAXNAMELENGTH];
	if (sname.find("/") != 0)
	{
		stem = dir;
		sitem = currentInode;
		for (int i = 0; i < v.size(); i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(stem.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * stem.inodeID[j], SEEK_SET);
					fread(&sitem, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * sitem.addr[0], SEEK_SET);
					fread(&stem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			sitem = rootInode;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&stem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size(); i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(stem.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * stem.inodeID[j], SEEK_SET);
						fread(&sitem, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * sitem.addr[0], SEEK_SET);
						fread(&stem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}

	v = SplitString(dname, "/");
	direct dtem;
	inode ditem, preItem;
	dtem = dir;
	ditem = currentInode;
	preItem = ditem;
	string filename = v[v.size() - 1];
	if (dname.find("/") != 0)
	{
		for (int i = 0; i < v.size() - 1; i++)
		{
			bool flag = false;
			strcpy(dirName, v[i].c_str());
			preItem = ditem;
			for (int j = 0; j < DIRMAXFILENUM; j++)
			{
				if (strcmp(dtem.fileName[j], dirName) == 0)
				{
					fseek(file, INODESTART + INODESIZE * dtem.inodeID[j], SEEK_SET);
					fread(&ditem, sizeof(inode), 1, file);
					fseek(file, DATASTART + BLOCKSIZE * ditem.addr[0], SEEK_SET);
					fread(&dtem, sizeof(direct), 1, file);
					flag = true;
					break;
				}
			}
			if (flag)
			{
				continue;
			}
			else
			{
				cout << "系统找不到指定路径" << endl;
				return;
			}
		}
	}
	else
	{
		if (v[1].compare("home") == 0)
		{
			ditem = rootInode;
			preItem = ditem;
			fseek(file, DATASTART + BLOCKSIZE * rootInode.addr[0], SEEK_SET);
			fread(&dtem, sizeof(direct), 1, file);
			for (int i = 2; i < v.size() - 1; i++)
			{
				bool flag = false;
				strcpy(dirName, v[i].c_str());
				preItem = ditem;
				for (int j = 0; j < DIRMAXFILENUM; j++)
				{
					if (strcmp(dtem.fileName[j], dirName) == 0)
					{
						fseek(file, INODESTART + INODESIZE * dtem.inodeID[j], SEEK_SET);
						fread(&ditem, sizeof(inode), 1, file);
						fseek(file, DATASTART + BLOCKSIZE * ditem.addr[0], SEEK_SET);
						fread(&dtem, sizeof(direct), 1, file);
						flag = true;
						break;
					}
				}
				if (flag)
				{
					continue;
				}
				else
				{
					cout << "系统找不到指定路径" << endl;
					return;
				}
			}
		}
		else
		{
			cout << "系统找不到指定路径" << endl;
			return;
		}
	}

	//检查权限
	if (!CheckWrite(sitem, "sub"))
	{
		cout << "您没有对该文件建立链接的权限" << endl;
		return;
	}
	if (!CheckWrite(ditem, "current"))
	{
		cout << "您没有对该文件夹进行操作的权限" << endl;
		return;
	}

	//检测能否创建文件
	if (superblock.spareInodeNum <= 0)
	{
		cout << "空间已满无法无法建立链接" << endl;
		return;
	}
	//检验是否有重名文件
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strcmp(filename.c_str(), dtem.fileName[i]) == 0)
		{
			cout << "存在同名文件，无法建立链接" << endl;
			return;
		}
	}
	//检查当前目录是否已满
	if (ditem.count >= DIRMAXFILENUM)
	{
		cout << "当前目录已满，无法建立链接" << endl;
		return;
	}

	//创建inode
	inode subInode;
	subInode.inodeNo = UpdateInodeMap();
	for (int i = 0; i < ADDRSIZE; i++)
	{
		subInode.addr[i] = sitem.addr[i];
	}
	for (int i = 0; i < 9; i++)
	{
		subInode.authority[i] = sitem.authority[i];
	}
	subInode.count = 1;
	subInode.fileSize = sitem.fileSize;
	subInode.groupId = sitem.groupId;
	subInode.ownerId = sitem.ownerId;
	strcpy(subInode.lastTime, getTime());
	subInode.mode = FILEMODE::LINK;
	fseek(file, INODESTART + INODESIZE * subInode.inodeNo, SEEK_SET);
	fwrite(&subInode, sizeof(inode), 1, file);

	//更新目录
	for (int i = 0; i < DIRMAXFILENUM; i++)
	{
		if (strlen(dtem.fileName[i]) == 0)
		{
			strcpy(dtem.fileName[i], filename.c_str());
			dtem.inodeID[i] = subInode.inodeNo;
			break;
		}
	}
	fseek(file, DATASTART + BLOCKSIZE * ditem.addr[0], SEEK_SET);
	fwrite(&dtem, sizeof(direct), 1, file);

}

void Detail()
{
	cout << "空闲块数量:" << superblock.spareBlockNum << endl
		<< "空闲inode数量:" << superblock.spareInodeNum << endl;
	cout << "当前inode使用情况(0未使用，1已使用):";
	for (int i = 0; i < INODETOTALNUM; i++)
		cout << inodeMap[i] << " ";
	cout << endl;
	cout << "当前空闲盘块指针:" << superblock.spareBlock[SPARESTACKNUM] << endl;
	cout << "当前空闲盘块栈:";
	for (int i = 0; i < SPARESTACKNUM; i++)
		cout << superblock.spareBlock[i] << " ";
	cout << endl;
}
void Command()
{
	vector<string> comm;
	string comma;
	getline(cin, comma);
	if (comma.length() == 0)
		return;
	comm = SplitString(comma, " ");
	switch (s_mapStringValues[comm[0]])
	{
	case detail:
		if (comm.size() > 1)
		{
			cout << "无效命令:detail无需参数" << endl;
			return;
		}
		Detail();
		break;
	case ln:
		if (comm.size() < 3)
		{
			cout << "无效命令:ln需要参数 ln [源文件或目录] [目标文件或目录]" << endl;
			return;
		}
		Ln(comm[1], comm[2]);
		break;
	case cp:
		if (comm.size() < 3)
		{
			cout << "无效命令:cp需要参数 cp [源文件] [目标文件夹]" << endl;
			return;
		}
		Cp(comm[1], comm[2]);
		break;
	case rm:
		if (comm.size() < 2)
		{
			cout << "无效命令:cp需要参数 cp [目标文件]" << endl;
			return;
		}
		Rm(comm[1]);
		break;
	case cat:
		if (comm.size() < 2)
		{
			cout << "无效命令:cat需要明确参数 cat [目标文件]" << endl;
			return;
		}
		Cat(comm[1]);
		break;
	case chgrp:
		if (comm.size() < 3)
		{
			cout << "无效命令:chgrp需要明确参数 chgrp [群组] [目标文件]" << endl;
			return;
		}
		Chgrp(comm[1], comm[2]);
		break;
	case chown:
		if (comm.size() < 3)
		{
			cout << "无效命令:chown需要明确参数 chown [拥有着] [目标文件]" << endl;
			return;
		}
		Chown(comm[1], comm[2]);
		break;
	case chmod:
		if (comm.size() < 3)
		{
			cout << "无效命令:chmod需要明确参数 chmod [类型] [目标文件]" << endl;
			return;
		}
		Chmod(comm[1], comm[2]);
		break;
	case help:
		if (comm.size() > 1)
		{
			cout << "无效命令:help无需参数" << endl;
			return;
		}
		Help();
		break;
	case exi:
		if (comm.size() > 1)
		{
			cout << "无效命令:exit无需参数" << endl;
			return;
		}
		Exit();
		break;
	case ls:
		if (comm.size() > 1)
		{
			cout << "无效命令:ls无需参数" << endl;
			return;
		}
		Ls();
		break;
	case mv:
		if (comm.size() < 3)
		{
			cout << "无效命令:mv需要明确参数 mv [源文件] [目标文件]" << endl;
			return;
		}
		Mv(comm[1], comm[2]);
		break;
	case pwd:
		if (comm.size() > 1)
		{
			cout << "无效命令:pwd无需参数" << endl;
			return;
		}
		Pwd();
		break;
	case cd:
		if (comm.size() < 2)
		{
			cout << "无效命令:cd需要明确参数" << endl;
			return;
		}
		Cd(comm[1]);
		break;
	case mkdir:
		if (comm.size() < 2)
		{
			cout << "无效命令:mkdir需要明确参数" << endl;
			return;
		}
		Mkdir(comm[1]);
		break;
	case rmdir:
		if (comm.size() < 2)
		{
			cout << "无效命令:rmdir需要明确参数" << endl;
			return;
		}
		Rmdir(comm[1]);
		break;
	case passwd:
		if (comm.size() > 1)
		{
			cout << "无效命令:passwd无需参数" << endl;
			return;
		}
		Passwd();
		break;
	case umask:
		if (comm.size() < 2)
		{
			Umask("");
		}
		else
			Umask(comm[1]);
		break;
	case vi:
		if (comm.size() < 3)
		{
			cout << "无效命令:vi需要明确参数 vi [文件名] [文件大小]" << endl;
			return;
		}
		Vi(comm[1], InputContent(comm[2]));
		break;
	default:
		cout << "无效命令,输入help寻求帮助" << endl;
		break;
	}
}

int main()
{
	//查找卷
	FILE* infile = fopen("yuyi.txt", "r");
	if (infile == NULL)
	{
		cout << "文件卷未找到，正在新建文件卷......" << endl;
		init();
	}
	//读取文件卷信息
	Mount();

	//登录
	string userName, psw;
	char userPsw[MAXNAMELENGTH], ch;

	while (true)
	{
		cout << "Enter UserName:";
		cin >> userName;
		cout << "Enter Password:";
		psw = Encryption();
		if (Login(userName, psw))
			break;
	}
	initialCommand();
	bool flag = true;
	while (flag)
	{
		Command();
		Prefix();
	}
	system("pause");
	return 0;
}