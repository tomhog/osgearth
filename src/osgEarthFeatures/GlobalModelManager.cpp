#include "GlobalModelManager"

GlobalModelManager ReplaceCurrentModel_Instance;

GlobalModelManager::GlobalModelManager()
{
}


GlobalModelManager::~GlobalModelManager()
{
}

GlobalModelManager * GlobalModelManager::getInstance(void)
{
	return &ReplaceCurrentModel_Instance;
}

void GlobalModelManager::Add_To_Replacment_Stack(ModelReplacmentdataPV Replacements)
{
	m_StackMutex.lock();
	m_ReplaceNodes.push(Replacements);
	m_StackMutex.unlock();
}

bool GlobalModelManager::Have_Replacement_Data(void)
{
	if (m_ReplaceNodes.size() > 0)
		return true;
	else
		return false;
}

ModelReplacmentdataPV GlobalModelManager::Test_Next_Replacement(void)
{
	return m_ReplaceNodes.top();
}

ModelReplacmentdataPV GlobalModelManager::Get_Next_Replacement_From_Stack(void)
{
	m_StackMutex.lock();
	ModelReplacmentdataPV Replacements = m_ReplaceNodes.top();
	m_ReplaceNodes.pop();
	m_StackMutex.unlock();
	return Replacements;
}

void GlobalModelManager::Add_To_Retired_Map(std::string NodeName, std::string XformName, osg::ref_ptr<osg::Node> oldNode)
{
	RetiredModelData MD(XformName, oldNode);
	m_RetiredGSModels.insert(std::pair<std::string, RetiredModelData>(NodeName,MD));
}

bool GlobalModelManager::Have_RetiredModel(std::string ModelName)
{

	m_RetiredMutex.lock();

	RetiredModelMap::iterator mi = m_RetiredGSModels.find(ModelName);
	if (mi == m_RetiredGSModels.end())
		return false;
	else
		return true;

	m_RetiredMutex.unlock();

}