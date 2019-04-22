/* UNIXV6PP 文件系统(主要是超块操作)代码裁剪. */
#include "FileSystem.hpp"

void FileSystem::LoadSuperBlock()
{
	User& u = Kernel::Instance().GetUser();
	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();
	Buf* pBuf;

	for (int i = 0; i < 2; i++)
	{
		int* p = (int *)&g_spb + i * 128;

		pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, FileSystem::SUPER_BLOCK_SECTOR_NUMBER + i);

		Utility::DWordCopy((int *)pBuf->b_addr, p, 128);

		bufMgr.Brelse(pBuf);
	}
	if (User::NOERROR != u.u_error)
	{
		Utility::Panic("Load SuperBlock Error....!\n");
	}

	this->m_Mount[0].m_dev = DeviceManager::ROOTDEV;
	this->m_Mount[0].m_spb = &g_spb;

	g_spb.s_flock = 0;
	g_spb.s_ilock = 0;
	g_spb.s_ronly = 0;
	g_spb.s_time = Time::time;
}