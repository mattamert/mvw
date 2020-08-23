#include "d3d12/ResourceGarbageCollector.h"

#include <assert.h>

void ResourceGarbageCollector::MarkAsGarbage(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                             uint64_t signalValue) {
  assert((m_garbage.size() > 0) ? m_garbage.front().signalValue <= signalValue : true);

  ResourceGarbageCollector::Garbage newGarbage;
  newGarbage.resource = std::move(resource);
  newGarbage.signalValue = signalValue;

  m_garbage.push(std::move(newGarbage));
}

void ResourceGarbageCollector::Cleanup(uint64_t signalValue) {
  while (m_garbage.size() > 0 && m_garbage.front().signalValue <= signalValue)
    m_garbage.pop();
}
