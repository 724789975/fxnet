namespace FxNet.Core
{
    /// <summary>
    /// FxNet 网络库自定义异常类型。
    /// 携带 ErrorCode，便于上层按错误码分类处理。
    /// </summary>
    public class FxNetException : Exception
    {
        /// <summary>关联的错误码</summary>
        public ErrorCode ErrorCode { get; }

        public FxNetException() : base("FxNet error") { ErrorCode = new ErrorCode(); }
        public FxNetException(string message) : base(message) { ErrorCode = new ErrorCode(); }
        public FxNetException(string message, Exception innerException) : base(message, innerException) { ErrorCode = new ErrorCode(); }
        public FxNetException(int code, string message) : base(message) { ErrorCode = new ErrorCode(code, message); }
        public FxNetException(ErrorCode errorCode) : base(errorCode.GetWhat()) { ErrorCode = errorCode; }
    }
}
