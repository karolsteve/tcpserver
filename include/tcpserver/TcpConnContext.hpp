#if !defined(TKS_CONTEXT)
#define TKS_CONTEXT

class TcpConnContext
{
private:
    /* data */
public:
    TcpConnContext(const TcpConnContext &) = delete;
    void operator=(const TcpConnContext &) = delete;
    virtual ~TcpConnContext() = default;

protected:
    TcpConnContext() = default;
    
};

#endif // TKS_CONTEXT
