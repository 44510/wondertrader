/*!
 * \file WTSBaseDataMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#include "WTSBaseDataMgr.h"

#include "../Share/WTSContractInfo.hpp"
#include "../Share/WTSCollection.hpp"
#include "../Share/WTSSessionInfo.hpp"

#include "../Share/StrUtil.hpp"
#include "../Share/StdUtils.hpp"

#include "WTSLogger.h"

#include <rapidjson/document.h>


const char* DEFAULT_HOLIDAY_TPL = "CHINA";

namespace rj = rapidjson;


WTSBaseDataMgr::WTSBaseDataMgr()
	: m_mapExchgContract(NULL)
	, m_mapSessions(NULL)
	, m_pCommodityMap(NULL)
{
	m_mapExchgContract = WTSExchgContract::create();
	m_mapSessions = WTSSessionMap::create();
	m_pCommodityMap = WTSCommodityMap::create();
}


WTSBaseDataMgr::~WTSBaseDataMgr()
{
	if (m_mapExchgContract)
	{
		m_mapExchgContract->release();
		m_mapExchgContract = NULL;
	}

	if (m_mapSessions)
	{
		m_mapSessions->release();
		m_mapSessions = NULL;
	}

	if (m_pCommodityMap)
	{
		m_pCommodityMap->release();
		m_pCommodityMap = NULL;
	}

}

WTSCommodityInfo* WTSBaseDataMgr::getCommodity(const char* exchgpid)
{
	return (WTSCommodityInfo*)m_pCommodityMap->get(exchgpid);
}


WTSCommodityInfo* WTSBaseDataMgr::getCommodity(const char* exchg, const char* pid)
{
	std::string key = StrUtil::printf("%s.%s", exchg, pid);
	if (m_pCommodityMap == NULL)
		return NULL;

	return (WTSCommodityInfo*)m_pCommodityMap->get(key);
}

WTSCommodityInfo* WTSBaseDataMgr::getCommodity(WTSContractInfo* ct)
{
	if (ct == NULL)
		return NULL;

	return getCommodity(ct->getExchg(), ct->getProduct());
}

WTSContractInfo* WTSBaseDataMgr::getContract(const char* code, const char* exchg)
{
	std::string realCode = code;	

	//���ֱ���ҵ���Ӧ���г����룬��ֱ��
	auto it = m_mapExchgContract->find(exchg);
	if(it != m_mapExchgContract->end())
	{
		WTSContractList* contractList = (WTSContractList*)it->second;
		WTSContractList::Iterator it = contractList->find(realCode);
		if(it != contractList->end())
		{
			return (WTSContractInfo*)it->second;
		}
	}
	else
	{
		it = m_mapExchgContract->begin();
		for(; it != m_mapExchgContract->end(); it++)
		{
			WTSContractList* contractList = (WTSContractList*)it->second;
			WTSContractList::Iterator it = contractList->find(realCode);
			if (it != contractList->end())
			{
				return (WTSContractInfo*)it->second;
			}
		}
	}

	return NULL;
}

WTSArray* WTSBaseDataMgr::getContracts(const char* exchg /* = "" */)
{
	WTSArray* ay = WTSArray::create();
	if(strlen(exchg) > 0)
	{
		auto it = m_mapExchgContract->find(exchg);
		if (it != m_mapExchgContract->end())
		{
			WTSContractList* contractList = (WTSContractList*)it->second;
			auto it2 = contractList->begin();
			for (; it2 != contractList->end(); it2++)
			{
				ay->append(it2->second, true);
			}
		}
	}
	else
	{
		auto it = m_mapExchgContract->begin();
		for (; it != m_mapExchgContract->end(); it++)
		{
			WTSContractList* contractList = (WTSContractList*)it->second;
			auto it2 = contractList->begin();
			for (; it2 != contractList->end(); it2++)
			{
				ay->append(it2->second, true);
			}
		}
	}

	return ay;
}

WTSArray* WTSBaseDataMgr::getAllSessions()
{
	WTSArray* ay = WTSArray::create();
	for (auto it = m_mapSessions->begin(); it != m_mapSessions->end(); it++)
	{
		ay->append(it->second, true);
	}
	return ay;
}

WTSSessionInfo* WTSBaseDataMgr::getSession(const char* sid)
{
	return (WTSSessionInfo*)m_mapSessions->get(sid);
}

WTSSessionInfo* WTSBaseDataMgr::getSessionByCode(const char* code, const char* exchg /* = "" */)
{
	WTSContractInfo* ct = getContract(code, exchg);
	if (ct == NULL)
		return NULL;

	WTSCommodityInfo* commInfo = getCommodity(ct);
	if (commInfo == NULL)
		return NULL;

	return getSession(commInfo->getSession());
}

bool WTSBaseDataMgr::isHoliday(const char* pid, uint32_t uDate, bool isTpl /* = false */)
{
	std::string tplid = pid;
	if (!isTpl)
		tplid = getTplIDByPID(pid);

	auto it = m_mapTradingDay.find(tplid);
	if(it != m_mapTradingDay.end())
	{
		const TradingDayTpl& tpl = it->second;
		return (tpl._holidays.find(uDate) != tpl._holidays.end());
	}

	return false;
}


void WTSBaseDataMgr::release()
{
	if (m_mapExchgContract)
	{
		m_mapExchgContract->release();
		m_mapExchgContract = NULL;
	}

	if (m_mapSessions)
	{
		m_mapSessions->release();
		m_mapSessions = NULL;
	}

	if (m_pCommodityMap)
	{
		m_pCommodityMap->release();
		m_pCommodityMap = NULL;
	}
}

bool WTSBaseDataMgr::loadSessions(const char* filename)
{
	if (!StdFile::exists(filename))
	{
		WTSLogger::error("����ʱ�������ļ� %s ������", filename);
		return false;
	}

	std::string content;
	StdFile::read_file_content(filename, content);
	rj::Document root;
	if (root.Parse(content.c_str()).HasParseError())
	{
		WTSLogger::error("����ʱ�������ļ�����ʧ��");
		return false;
	}

	for(auto& m : root.GetObject())
	{
		const std::string& id = m.name.GetString();
		const rj::Value& jVal = m.value;

		const char* name = jVal["name"].GetString();
		int32_t offset = jVal["offset"].GetInt();

		WTSSessionInfo* sInfo = WTSSessionInfo::create(id.c_str(), name, offset);

		if (!jVal["auction"].IsNull())
		{
			const rj::Value& jAuc = jVal["auction"];
			sInfo->setAuctionTime(jAuc["from"].GetUint(), jAuc["to"].GetUint());
		}

		const rj::Value& jSecs = jVal["sections"];
		if (jSecs.IsNull() || !jSecs.IsArray())
			continue;

		for (const rj::Value& jSec : jSecs.GetArray())
		{
			sInfo->addTradingSection(jSec["from"].GetUint(), jSec["to"].GetUint());
		}

		m_mapSessions->add(id, sInfo);
	}

	return true;
}

bool WTSBaseDataMgr::loadCommodities(const char* filename)
{
	if (!StdFile::exists(filename))
	{
		WTSLogger::error("Ʒ�����������ļ� %s ������", filename);
		return false;
	}

	std::string content;
	StdFile::read_file_content(filename, content);
	rj::Document root;
	if (root.Parse(content.c_str()).HasParseError())
	{
		WTSLogger::error("Ʒ�����������ļ�����ʧ��");
		return false;
	}

	for(auto& m : root.GetObject())
	{
		const std::string& exchg = m.name.GetString();
		const rj::Value& jExchg = m.value;

		for (auto& mExchg : jExchg.GetObject())
		{
			const std::string& pid = mExchg.name.GetString();
			const rj::Value& jPInfo = mExchg.value;

			const char* name = jPInfo["name"].GetString();
			const char* sid = jPInfo["session"].GetString();
			const char* hid = jPInfo["holiday"].GetString();

			if (strlen(sid) == 0)
			{
				WTSLogger::error("Ʒ��%sû���ҵ���Ӧ�ĻỰID", pid.c_str());
				continue;
			}

			WTSCommodityInfo* pCommInfo = WTSCommodityInfo::create(pid.c_str(), name, exchg.c_str(), sid, hid);
			pCommInfo->setPrecision(jPInfo["precision"].GetUint());
			pCommInfo->setPriceTick(jPInfo["pricetick"].GetDouble());
			pCommInfo->setVolScale(jPInfo["volscale"].GetUint());

			if (!jPInfo["category"].IsNull())
				pCommInfo->setCategory((ContractCategory)jPInfo["category"].GetUint());
			else
				pCommInfo->setCategory(CC_Future);

			pCommInfo->setCoverMode((CoverMode)jPInfo["covermode"].GetUint());
			pCommInfo->setPriceMode((PriceMode)jPInfo["pricemode"].GetUint());

			std::string key = StrUtil::printf("%s.%s", exchg.c_str(), pid.c_str());
			if (m_pCommodityMap == NULL)
				m_pCommodityMap = WTSCommodityMap::create();

			m_pCommodityMap->add(key, pCommInfo, false);

			m_mapSessionCode[sid].insert(key);
		}
	}

	return true;
}

bool WTSBaseDataMgr::loadContracts(const char* filename)
{
	if (!StdFile::exists(filename))
	{
		WTSLogger::error("��Լ�б��ļ� %s ������", filename);
		return false;
	}

	std::string content;
	StdFile::read_file_content(filename, content);
	rj::Document root;
	if (root.Parse(content.c_str()).HasParseError())
	{
		WTSLogger::error("��Լ�б��ļ�����ʧ��");
		return false;
	}

	for(auto& m : root.GetObject())
	{
		const std::string& exchg = m.name.GetString();
		const rj::Value& jExchg = m.value;

		for(auto& cMember : jExchg.GetObject())
		{
			const std::string& code = cMember.name.GetString();
			const rj::Value& jcInfo = cMember.value;

			WTSCommodityInfo* commInfo = getCommodity(jcInfo["exchg"].GetString(), jcInfo["product"].GetString());
			if (commInfo == NULL)
				continue;

			WTSContractInfo* cInfo = WTSContractInfo::create(code.c_str(),
				jcInfo["name"].GetString(),
				jcInfo["exchg"].GetString(),
				jcInfo["product"].GetString());

			uint32_t maxMktQty = 0;
			uint32_t maxLmtQty = 0;
			if (jcInfo.HasMember("maxmarketqty"))
				maxMktQty = jcInfo["maxmarketqty"].GetUint();
			if (jcInfo.HasMember("maxlimitqty"))
				maxLmtQty = jcInfo["maxlimitqty"].GetUint();
			cInfo->setVolumnLimits(maxMktQty, maxLmtQty);

			WTSContractList* contractList = (WTSContractList*)m_mapExchgContract->get(cInfo->getExchg());
			if (contractList == NULL)
			{
				contractList = WTSContractList::create();
				m_mapExchgContract->add(cInfo->getExchg(), contractList, false);
			}
			contractList->add(cInfo->getCode(), cInfo, false);

			commInfo->addCode(code.c_str());
		}
	}

	return true;
}

bool WTSBaseDataMgr::loadHolidays(const char* filename)
{
	if (!StdFile::exists(filename))
	{
		WTSLogger::error("�ڼ����ļ� %s ������", filename);
		return false;
	}

	std::string content;
	StdFile::read_file_content(filename, content);
	rj::Document root;
	if (root.Parse(content.c_str()).HasParseError())
	{
		WTSLogger::error("�ڼ����ļ�����ʧ��");
		return false;
	}

	for (auto& m : root.GetObject())
	{
		const std::string& hid = m.name.GetString();
		const rj::Value& jHolidays = m.value;
		if(!jHolidays.IsArray())
			continue;

		TradingDayTpl& trdDayTpl = m_mapTradingDay[hid];
		for(const rj::Value& hItem : jHolidays.GetArray())
		{
			trdDayTpl._holidays.insert(hItem.GetUint());
		}
	}

	return true;
}

uint64_t WTSBaseDataMgr::getBoundaryTime(const char* stdPID, uint32_t tDate, bool isSession /* = false */, bool isStart /* = true */)
{
	if(tDate == 0)
		tDate = TimeUtils::getCurDate();
	
	std::string tplid = stdPID;
	bool isTpl = false;
	WTSSessionInfo* sInfo = NULL;
	if (isSession)
	{
		sInfo = getSession(stdPID);
		tplid = DEFAULT_HOLIDAY_TPL;
		isTpl = true;
	}
	else
	{
		WTSCommodityInfo* cInfo = getCommodity(stdPID);
		if (cInfo == NULL)
			return 0;

		sInfo = getSession(cInfo->getSession());
	}

	if (sInfo == NULL)
		return 0;

	uint32_t weekday = TimeUtils::getWeekDay(tDate);
	if (weekday == 6 || weekday == 0)
	{
		if (isStart)
			tDate = getNextTDate(tplid.c_str(), tDate, 1, isTpl);
		else
			tDate = getPrevTDate(tplid.c_str(), tDate, 1, isTpl);
	}

	//��ƫ�Ƶ���򵥣�ֻ��Ҫֱ�ӷ��ؿ��̺�����ʱ�伴��
	if (sInfo->getOffsetMins() == 0)
	{
		if (isStart)
			return (uint64_t)tDate * 10000 + sInfo->getOpenTime();
		else
			return (uint64_t)tDate * 10000 + sInfo->getCloseTime();
	}

	if(sInfo->getOffsetMins() < 0)
	{
		//��ǰƫ�ƣ����ǽ������ƺ�һ����������
		//����Ƚϼ򵥣�ֻ��Ҫ����Ȼ��ȡ����
		if (isStart)
			return (uint64_t)tDate * 10000 + sInfo->getOpenTime();
		else
			return (uint64_t)TimeUtils::getNextDate(tDate) * 10000 + sInfo->getCloseTime();
	}
	else
	{
		//����ƫ�ƣ�һ������ڻ�ҹ�̶����������ҹ�����ǵڶ���������
		//����Ƚϸ��ӣ���Ҫ�ڼ��պ��һ��ı߽���鷳����һ���������һ��
		//�������Ψһ����ľ��ǣ�����ʱ�䲻��Ҫ����
		if(!isStart)
			return (uint64_t)tDate * 10000 + sInfo->getCloseTime();

		//�뵽һ���򵥵İ취�����ǲ�����ô������ʼʱ��һ������һ�������յ�����
		//������ֻ��Ҫ�õ���һ�������ռ���
		tDate = getPrevTDate(tplid.c_str(), tDate, 1, isTpl);
		return (uint64_t)tDate * 10000 + sInfo->getOpenTime();
	}
}

uint32_t WTSBaseDataMgr::calcTradingDate(const char* stdPID, uint32_t uDate, uint32_t uTime, bool isSession /* = false */)
{
	if (uDate == 0)
	{
		TimeUtils::getDateTime(uDate, uTime);
		uTime /= 100000;
	}

	std::string tplid = stdPID;
	bool isTpl = false;
	WTSSessionInfo* sInfo = NULL;
	if(isSession)
	{
		sInfo = getSession(stdPID);
		tplid = DEFAULT_HOLIDAY_TPL;
		isTpl = true;
	}
	else
	{
		WTSCommodityInfo* cInfo = getCommodity(stdPID);
		if (cInfo == NULL)
			return uDate;
		
		sInfo = getSession(cInfo->getSession());
	}

	if (sInfo == NULL)
		return uDate;
	

	uint32_t weekday = TimeUtils::getWeekDay(uDate);

	uint32_t offMin = sInfo->offsetTime(uTime);
	if (sInfo->getOffsetMins() > 0)
	{
		//������ƫ�ƣ��ҵ�ǰʱ�����ƫ��ʱ�䣬˵����������
		//��ʱ������=��һ��������
		if (uTime > offMin)
		{
			//�磬20151016 23:00��ƫ��300���ӣ�Ϊ5:00
			return getNextTDate(tplid.c_str(), uDate, 1, isTpl);
		}
		else if (weekday == 6 || weekday == 0)
		{
			//�磬20151017 1:00��������������Ϊ20151019
			return getNextTDate(tplid.c_str(), uDate, 1, isTpl);
		}
	}
	else if (sInfo->getOffsetMins() < 0)
	{
		//�����ǰƫ�ƣ��ҵ�ǰʱ��С��ƫ��ʱ�䣬˵������ǰһ��������
		//��ʱ������=ǰһ��������
		if (uTime < offMin)
		{
			//��20151017 1:00��ƫ��-300���ӣ�Ϊ20:00
			return getPrevTDate(tplid.c_str(), uDate, 1, isTpl);
		}
		else if (weekday == 6 || weekday == 0)
		{
			//��Ϊ��ǰƫ�ƣ��������ĩ����ֱ�ӵ���һ��������
			return getNextTDate(tplid.c_str(), uDate, 1, isTpl);
		}
	}
	else if (weekday == 6 || weekday == 0)
	{
		//���û��ƫ�ƣ�������ĩ����ֱ�Ӷ�ȡ��һ��������
		return getNextTDate(tplid.c_str(), uDate, 1, isTpl);;
	}

	//���������������=��Ȼ��
	return uDate;
}

uint32_t WTSBaseDataMgr::getTradingDate(const char* pid, uint32_t uOffDate /* = 0 */, uint32_t uOffMinute /* = 0 */, bool isTpl /* = false */)
{
	const char* tplID = isTpl ? pid : getTplIDByPID(pid);

	uint32_t curDate = TimeUtils::getCurDate();
	auto it = m_mapTradingDay.find(tplID);
	if (it == m_mapTradingDay.end())
	{
		return curDate;
	}

	TradingDayTpl& tpl = it->second;
	if (tpl._cur_tdate != 0 && uOffDate == 0)
		return tpl._cur_tdate;

	if (uOffDate == 0)
		uOffDate = curDate;

	uint32_t weekday = TimeUtils::getWeekDay(uOffDate);

	if (weekday == 6 || weekday == 0)
	{
		//���û��ƫ�ƣ�������ĩ����ֱ�Ӷ�ȡ��һ��������
		tpl._cur_tdate = getNextTDate(tplID, uOffDate, 1, true);
		uOffDate = tpl._cur_tdate;
	}

	//���������������=��Ȼ��
	return uOffDate;
}

uint32_t WTSBaseDataMgr::getNextTDate(const char* pid, uint32_t uDate, int days /* = 1 */, bool isTpl /* = false */)
{
	uint32_t curDate = uDate;
	int left = days;
	while (true)
	{
		tm t;
		memset(&t, 0, sizeof(tm));
		t.tm_year = curDate / 10000 - 1900;
		t.tm_mon = (curDate % 10000) / 100 - 1;
		t.tm_mday = curDate % 100;
		//t.tm_isdst 	
		time_t ts = mktime(&t);
		ts += 86400;

		tm* newT = localtime(&ts);
		curDate = (newT->tm_year + 1900) * 10000 + (newT->tm_mon + 1) * 100 + newT->tm_mday;
		if (newT->tm_wday != 0 && newT->tm_wday != 6 && !isHoliday(pid, curDate, isTpl))
		{
			//���������ĩ��Ҳ���ǽڼ��գ���ʣ�������-1
			left--;
			if (left == 0)
				return curDate;
		}
	}
}

uint32_t WTSBaseDataMgr::getPrevTDate(const char* pid, uint32_t uDate, int days /* = 1 */, bool isTpl /* = false */)
{
	uint32_t curDate = uDate;
	int left = days;
	while (true)
	{
		tm t;
		memset(&t, 0, sizeof(tm));
		t.tm_year = curDate / 10000 - 1900;
		t.tm_mon = (curDate % 10000) / 100 - 1;
		t.tm_mday = curDate % 100;
		//t.tm_isdst 	
		time_t ts = mktime(&t);
		ts -= 86400;

		tm* newT = localtime(&ts);
		curDate = (newT->tm_year + 1900) * 10000 + (newT->tm_mon + 1) * 100 + newT->tm_mday;
		if (newT->tm_wday != 0 && newT->tm_wday != 6 && !isHoliday(pid, curDate, isTpl))
		{
			//���������ĩ��Ҳ���ǽڼ��գ���ʣ�������-1
			left--;
			if (left == 0)
				return curDate;
		}
	}
}

bool WTSBaseDataMgr::isTradingDate(const char* pid, uint32_t uDate, bool isTpl /* = false */)
{
	uint32_t wd = TimeUtils::getWeekDay(uDate);
	if (wd != 0 && wd != 6 && !isHoliday(pid, uDate, isTpl))
	{
		return true;
	}

	return false;
}

void WTSBaseDataMgr::setTradingDate(const char* pid, uint32_t uDate, bool isTpl /* = false */)
{
	std::string tplID = pid;
	if (!isTpl)
		tplID = getTplIDByPID(pid);

	auto it = m_mapTradingDay.find(tplID);
	if (it == m_mapTradingDay.end())
		return;

	TradingDayTpl& tpl = it->second;
	tpl._cur_tdate = uDate;
}


CodeSet* WTSBaseDataMgr::getSessionComms(const char* sid)
{
	auto it = m_mapSessionCode.find(sid);
	if (it == m_mapSessionCode.end())
		return NULL;

	return &it->second;
}

const char* WTSBaseDataMgr::getTplIDByPID(const char* pid)
{
	const StringVector& ay = StrUtil::split(pid, ".");
	WTSCommodityInfo* commInfo = getCommodity(ay[0].c_str(), ay[1].c_str());
	if (commInfo == NULL)
		return "";

	return commInfo->getTradingTpl();
}