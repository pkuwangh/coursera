#ifndef PROCMAP_H
#define PROCMAP_H

class ProcessorMap {
  public:
    ProcessorMap();
    ~ProcessorMap();

    int LogicalToPhysical(int lproc) const;
    int PhysicalToLogical(int pproc) const;
    int NumberOfProcessors() const { return m_nProcs; }

    int operator[]( int index ) const { return LogicalToPhysical( index ); }

    void BindToPhysicalCPU( int pproc ) const;

  private:
    void IntegrityCheck() const;
    int DetermineNumberOfProcessors();

    int * m_p_nProcessor_Ids;
    int m_nProcs;

};

#endif // #ifndef PROCMAP_H
