namespace FxNet.Util
{
    /// <summary>
    /// 泛型单例模板，采用双重检查锁保证线程安全。
    /// 子类继承后即可通过 Singleton&lt;T&gt;.Instance 访问单例。
    /// </summary>
    public abstract class Singleton<T> where T : class, new()
    {
        private static T? _instance;
        private static readonly object _syncRoot = new object();

        public static T? Instance => _instance;

        /// <summary>创建单例实例（线程安全），返回是否成功创建</summary>
        public static bool CreateInstance()
        {
            if (_instance == null)
            {
                lock (_syncRoot)
                {
                    if (_instance == null)
                    {
                        _instance = new T();
                        return true;
                    }
                }
            }
            return false;
        }

        /// <summary>使用指定实例创建单例</summary>
        public static bool CreateInstance(T instance)
        {
            if (_instance == null)
            {
                lock (_syncRoot)
                {
                    if (_instance == null)
                    {
                        _instance = instance;
                        return true;
                    }
                }
            }
            return false;
        }

        /// <summary>销毁单例实例</summary>
        public static bool DestroyInstance()
        {
            lock (_syncRoot)
            {
                if (_instance != null)
                {
                    _instance = null;
                    return true;
                }
                return false;
            }
        }
    }
}
