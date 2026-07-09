namespace FxNet.Util
{
    /// <summary>
    /// 对象池，预分配固定数量的对象实例，通过空闲链表进行分配/回收。
    /// 避免频繁 GC，适用于高频创建/销毁的场景。
    /// Free 操作通过 Dictionary 实现 O(1) 查找。
    /// </summary>
    public class Pool<T> where T : class, new()
    {
        public const int Size = 256; // 池容量

        private class Element
        {
            public T Value = default!;
            public Element? Next;
            public int UseCount;
        }

        private readonly Element[] _elements;
        private Element? _freeNode;
        private readonly Dictionary<T, int> _itemToIndex; // O(1) 查找索引

        public Pool()
        {
            _elements = new Element[Size];
            _freeNode = null;
            _itemToIndex = new Dictionary<T, int>(Size);
            Init();
        }

        private void Init()
        {
            for (int i = 0; i < Size; i++)
            {
                _elements[i] = new Element { Value = new T(), Next = null, UseCount = 0 };
                if (i < Size - 1)
                    _elements[i].Next = _elements[i + 1];
                _itemToIndex[_elements[i].Value] = i;
            }
            _freeNode = _elements[0];
        }

        /// <summary>从池中分配一个对象，无可用对象时返回 null</summary>
        public T? Allocate()
        {
            if (_freeNode == null) return null;

            var element = _freeNode;
            _freeNode = _freeNode.Next;
            element.Next = null;
            element.UseCount = 1;
            return element.Value;
        }

        /// <summary>将对象归还到池中（O(1) 查找）</summary>
        public void Free(T item)
        {
            if (_itemToIndex.TryGetValue(item, out int index))
            {
                _elements[index].UseCount = 0;
                _elements[index].Next = _freeNode;
                _freeNode = _elements[index];
            }
        }
    }

    /// <summary>
    /// 线程安全的对象池队列，基于 CAS 锁保护分配/回收操作。
    /// 适用于多线程环境下的对象复用。
    /// Free 操作通过 Dictionary 实现 O(1) 查找。
    /// </summary>
    public class CasLockQueue<T> where T : class, new()
    {
        public const int Size = 256; // 池容量

        private class Element
        {
            public T Value = default!;
            public Element? Next;
            public int UseCount;
        }

        private readonly Element[] _elements;
        private Element? _freeNode;
        private readonly CasLock _lock = new CasLock();
        private readonly Dictionary<T, int> _itemToIndex; // O(1) 查找索引

        public CasLockQueue()
        {
            _elements = new Element[Size];
            _freeNode = null;
            _itemToIndex = new Dictionary<T, int>(Size);
            Init();
        }

        private void Init()
        {
            for (int i = 0; i < Size; i++)
            {
                _elements[i] = new Element { Value = new T(), Next = null, UseCount = 0 };
                if (i < Size - 1)
                    _elements[i].Next = _elements[i + 1];
                _itemToIndex[_elements[i].Value] = i;
            }
            _freeNode = _elements[0];
        }

        public T? Allocate()
        {
            using (new LockScope(_lock))
            {
                if (_freeNode == null) return null;

                var element = _freeNode;
                _freeNode = _freeNode.Next;
                element.Next = null;
                element.UseCount = 1;
                return element.Value;
            }
        }

        public void Free(T item)
        {
            using (new LockScope(_lock))
            {
                if (_itemToIndex.TryGetValue(item, out int index))
                {
                    _elements[index].UseCount = 0;
                    _elements[index].Next = _freeNode;
                    _freeNode = _elements[index];
                }
            }
        }
    }
}
