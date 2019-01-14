/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/**
 * @brief : DAG(Directed Acyclic Graph) basic implement
 * @author: jimmyshi
 * @date: 2019-1-8
 */

#include "DAG.h"

using namespace std;
using namespace dev;
using namespace dev::blockverifier;

DAG::~DAG()
{
    clear();
}

void DAG::init(ID _maxSize)
{
    clear();
    for (ID i = 0; i < _maxSize; i++)
        m_vtxs.emplace_back(make_shared<Vertex>());
    m_totalVtxs = _maxSize;
}

void DAG::addEdge(ID _f, ID _t)
{
    if (_f >= m_vtxs.size() && _t >= m_vtxs.size())
        return;
    m_vtxs[_f]->outEdge.emplace_back(_t);
    m_vtxs[_t]->inDegree += 1;
    // PARA_LOG(TRACE) << LOG_BADGE("DAG") << LOG_DESC("Add edge") << LOG_KV("from", _f)
    //                << LOG_KV("to", _t);
}

void DAG::generate()
{
    for (ID id = 0; id < m_vtxs.size(); id++)
    {
        if (m_vtxs[id]->inDegree == 0)
            m_topLevel.push(id);
    }

    // PARA_LOG(TRACE) << LOG_BADGE("DAG") << LOG_DESC("generate")
    //                << LOG_KV("queueSize", m_topLevel.size());
    // for (ID id = 0; id < m_vtxs.size(); id++)
    // printVtx(id);
}

ID DAG::pop()
{
    Guard l(x_topLevel);
    if (m_topLevel.empty())
        return INVALID_ID;

    ID top = m_topLevel.front();
    m_topLevel.pop();
    return top;
}

ID DAG::waitPop()
{
    std::unique_lock<std::mutex> ul(x_topLevel);
    while (m_topLevel.empty())
    {
        if (m_totalConsume >= m_totalVtxs)
            return INVALID_ID;
        else
            cv_topLevel.wait(ul);
    }

    ID top = m_topLevel.front();
    m_topLevel.pop();
    return top;
}

void DAG::consume(ID _id)
{
    ID producedNum = 0;
    for (ID id : m_vtxs[_id]->outEdge)
    {
        auto vtx = m_vtxs[id];
        {
            WriteGuard l(vtx->vtxLock);
            vtx->inDegree -= 1;
        }
        if (vtx->inDegree == 0)
        {
            Guard l(x_topLevel);
            m_topLevel.push(id);
            producedNum++;
            if (producedNum >= 1)
                cv_topLevel.notify_one();  // await other thread
        }
    }

    Guard l(x_topLevel);
    m_totalConsume += 1;
    if (m_totalConsume >= m_totalVtxs)
        cv_topLevel.notify_all();  // If DAG reach the end, awake all waitPop thread to exit

    // PARA_LOG(TRACE) << LOG_BADGE("DAG") << LOG_DESC("consumed")
    //                << LOG_KV("queueSize", m_topLevel.size());
    // for (ID id = 0; id < m_vtxs.size(); id++)
    // printVtx(id);
}

void DAG::clear()
{
    m_vtxs = std::vector<std::shared_ptr<Vertex>>();
    Guard l(x_topLevel);
    m_topLevel = queue<ID>();
}

void DAG::printVtx(ID _id)
{
    for (ID id : m_vtxs[_id]->outEdge)
    {
        PARA_LOG(TRACE) << LOG_BADGE("DAG") << LOG_DESC("VertexEdge") << LOG_KV("ID", _id)
                        << LOG_KV("inDegree", m_vtxs[_id]->inDegree) << LOG_KV("edge", id);
    }
}