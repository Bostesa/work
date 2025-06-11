import React, { useState } from 'react';

const EnoLogo = ({ onClick }) => {
  const [showBubble, setShowBubble] = useState(false);

  const handleMouseEnter = () => {
    setShowBubble(true);
  };

  const handleMouseLeave = () => {
    setShowBubble(false);
  };

  return (
    <div className="eno-chatbot-container">
      <div 
        className="eno-logo" 
        onClick={onClick}
        onMouseEnter={handleMouseEnter}
        onMouseLeave={handleMouseLeave}
      >
        <div className="eno-circle">
          <span className="eno-text">eno</span>
        </div>
        {showBubble && (
          <div className="chat-bubble">
            <span>Hi! I'm Eno. How can I help?</span>
          </div>
        )}
      </div>
    </div>
  );
};

export default EnoLogo;