#pragma once

class Node;

class ILoader
{
  public:
    virtual ~ILoader() = 0;

    virtual void handle_read(Node& node) = 0;
    virtual void handle_read(Node& node, Key key) = 0;

    virtual void handle_write(Node& node) = 0;
    virtual void handle_write(Node& node, Key key) = 0;

    virtual void handle_reset(Node& node) = 0;
    virtual void handle_refresh(Node& node) = 0;
};
