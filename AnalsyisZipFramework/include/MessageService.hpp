#ifndef MESSAGESVC_HPP
#define MESSAGESVC_HPP

#include <iomanip>
#include <iostream>

#define RESET "\033[0m"
#define BLACK "\033[30m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

namespace MsgSvc {
  enum Level { Debug = 0, Info = 1, Warning = 2, Error = 3 };
}

// using namespace Messages;

class MessageService {
public:
  MessageService() = default;

  static void          Line();
  static void          emptyLine();
  static void          Debug( bool flag = true );
  static bool          DEBUG;
  static void          setKillBelow( MsgSvc::Level level );
  static MsgSvc::Level LEVEL;

  template <typename strings>
  static void Print( strings str = "" );

  template <typename First, typename... strings>
  static void Print( First firststr, strings... others );

  template <typename... strings>
  static void Info( strings... infos );

  template <typename... strings>
  static void Warning( strings... warnings );

  template <typename... strings>
  static void Error( strings... errors );

  template <typename... strings>
  static void Debug( strings... debugs );

private:
  static const int m_line  = 150;
  static const int m_width = 1;
};

inline bool MessageService::DEBUG = false;

inline MsgSvc::Level MessageService::LEVEL = MsgSvc::Level::Debug;

inline void MessageService::Line() {
  if ( MsgSvc::Level::Info < LEVEL ) return;

  std::cout << std::string( m_line, '=' ) << "\n";
}

inline void MessageService::emptyLine() {
  if ( MsgSvc::Level::Info < LEVEL ) return;

  std::cout << "\n";
}

inline void MessageService::Debug( bool flag ) {
  DEBUG = flag;
  LEVEL = MsgSvc::Level::Debug;
}
inline void MessageService::setKillBelow( MsgSvc::Level level ) { LEVEL = level; }

template <typename strings>
inline void MessageService::Print( strings str ) {
  std::cout << str << "";
}

template <typename First, typename... strings>
void MessageService::Print( First firststr, strings... others ) {
  Print( firststr );
  Print( others... );
}

template <typename... strings>
void MessageService::Info( strings... infos ) {
  if ( MsgSvc::Level::Info < LEVEL ) return;

  std::cout << CYAN;

  Print( "INFO\t", infos... );

  std::cout << std::setw( 0 ) << RESET << "\n";
}

template <typename... strings>
void MessageService::Warning( strings... warnings ) {
  if ( MsgSvc::Level::Warning < LEVEL ) return;

  std::cout << MAGENTA;

  Print( "WARNING\t", warnings... );

  std::cout << std::setw( 0 ) << RESET << "\n";
}

template <typename... strings>
void MessageService::Error( strings... errors ) {
  std::cout << RED;

  Print( "ERROR\t", errors... );

  std::cout << std::setw( 0 ) << RESET << "\n";
}

template <typename... strings>
void MessageService::Debug( strings... debugs ) {
  if ( MsgSvc::Level::Debug < LEVEL ) return;
  if ( DEBUG ) {
    std::cout << YELLOW;

    Print( "DEBUG\t", debugs... );

    std::cout << std::setw( 0 ) << RESET << "\n";
  }
}

#endif

#define INFO(msg...) do { MessageService::Info(__PRETTY_FUNCTION__ , ":", __LINE__, " ", msg); } while (0)
#define WARNING(msg...) do { MessageService::Warning(__PRETTY_FUNCTION__ , ":", __LINE__, " ", msg); } while (0)
#define ERROR(msg...) do { MessageService::Error(__PRETTY_FUNCTION__ , ":", __LINE__, " ", msg); } while (0)
#define DEBUG(msg...) do { MessageService::Debug(__PRETTY_FUNCTION__ , ":", __LINE__, " ", msg); } while (0)
